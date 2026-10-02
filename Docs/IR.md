# IR

The compiler's intermediate representation, between the front end (lexer, parser, resolver, typer) and the backends.
One IR serves every target: SPIR-V, HLSL and MSL for shaders, and bytecode for the script VM.

What it's for:

- **Close to every target.** Each IR construct maps to something each backend can emit directly, without having to
  rediscover structure the front end already knew.
- **Easy to reason about and optimize.** Basic blocks in a control flow graph, three-address instructions, exact types.
- **Simple types.** Scalars, vectors and matrices are values; arrays and structs are memory.
- **SSA optional.** The base form is SSA for everything but variables, which are memory. Full SSA is one pass away and
  never required.

## Structure

```
module
├── globals      typed memory: constants, private data, shader inputs and outputs, script memory
└── functions
    ├── parameters
    ├── locals       function memory, in their own list, indexed densely
    └── blocks       in structured order, the entry block first
        └── instructions, ending in exactly one terminator
```

An instruction produces at most one value, which never changes. Mutable variables, parameters included, are locals
accessed with `load` and `store`.

## Types

The IR uses the front end's types, which are unique and compared by pointer, plus pointer and function types.

| Kind | Examples | Class |
|---|---|---|
| Scalar | `bool`, `int`, `uint`, `float` | value |
| Vector | `float2` to `float4`, `int3`, `bool4` | value |
| Matrix | `float4x4`, `float3x4` (float only) | value |
| Pointer | `*function float4`, `*memory S` | value |
| Array | `[3]float2` | memory |
| Struct | `S` | memory |
| Function | `(uint, *function S) -> float4` | |

- **Values** are what instructions produce and take. Matrices are values too, however large.
- **Arrays and structs only exist in memory.** No instruction produces one. Their parts are reached with `access` and
  loaded or stored; whole objects are copied with `copy`.
- **No implicit conversions.** Operands have exactly the types an op requires. The front end inserts every `convert`
  and splat.
- **Matrices are column-major.** `floatCxR` has C columns, each a vector of R floats, like SPIR-V, MSL and GLSL.
  Indexing a matrix gives a column, and `M * v` is the math convention. HLSL's names and `mul` order are transposed
  relative to this; its emitter swaps them.
- **`bool`** is a value type. In memory it's 4 bytes, 0 or 1.

## Pointers

Pointers are ordinary values: they can be stored, loaded, passed, returned, selected and compared. A pointer type has
an address space and a pointee type.

| Address space | Holds |
|---|---|
| `function` | locals |
| `private` | mutable globals |
| `constant` | read-only globals with an initializer |
| `input`, `output` | shader interface globals, used only by entry wrappers |
| `memory` | scripts' linear memory: a 4 GB space indexed by 32-bit pointers, like wasm |

More come with their features: `uniform`, `storage` and `workgroup` for shader resources, and a physical storage space
for buffer device addresses.

Pointer operations:

- `access p, i...` steps into the object `p` points to: a struct field (constant index), an array element, a matrix
  column or a vector component. Its result points to that part.
- `offset p, n` moves `p` by `n` objects of its pointee type, C's `p + n`. (Reserved, not implemented yet.)
- `ptr_to_int`, `int_to_ptr` convert between pointers and integers. (Reserved, not implemented yet.)

`access` always stays within the object it started from; `offset` and the conversions don't.

### Pointers in shaders

Until buffer device addresses, shader backends take a subset: every pointer comes from a variable followed by
`access` steps. Shader code can't store pointers in memory, select or merge them, or use `offset`, `ptr_to_int` and
`int_to_ptr`. This is what SPIR-V allows without variable pointers, and what HLSL, which has no pointers, can print as
lvalues like `a.b[i].c`. The front end rejects pointers in shaders; a target check in the validator catches passes that
break the subset.

## Values

Everything in a module (instructions, blocks, parameters, locals, globals, constants and functions) lives in one pool
of 32-byte values that refer to each other by 32-bit index. Allocating and freeing are O(1), and references are half
the size of pointers. The pool is chunked so values don't move as it grows. The module is short-lived, so space that
passes abandon isn't reclaimed.

```cpp
typedef uint32 IRRef; // an index into the module's pool, 0 is none

enum class IRValueKind : uint8 { FREE, INSTRUCTION, BLOCK, PARAMETER, LOCAL, GLOBAL, CONSTANT, FUNCTION };

struct IRValue
{
	IRValueKind kind;
	IROp op;          // INSTRUCTION
	uint8 sub_op;     // INSTRUCTION: e.g. which intrinsic
	uint8 flags;
	IRRef parent;     // INSTRUCTION: its block. BLOCK, PARAMETER, LOCAL: their function. FREE: the next free value
	Type* type;       // the value's type; pointer types for locals and globals; nullptr for blocks and instructions
	                  // without a result
	union             // 16 bytes
	{
		struct { uint32 operands; uint16 operand_count; IRRef prev, next; } instruction;
		struct { IRRef first, last, prev, next; } block;
		struct { uint32 index; IRRef next; } parameter;
		struct { Constant* initializer; IRRef next; uint32 index; } local;
		struct { union { Constant* initializer; ShaderIO* io; }; Atom name; IRRef next; } global;
		struct { Constant* constant; IRRef next; } constant;
		struct { IRFunctionInfo* info; IRRef first_block, last_block; } function;
	};
};
```

- **Operands** are `IRRef`s in one module-wide array; an instruction holds an offset into it and a count. Any value can
  be an operand: an instruction's result, a parameter, a local or global (as a pointer), a constant, a function (for
  `call`) or a block (for branches).
- **Constants** are interned per module: one value per distinct constant. Indices that have to be compile-time
  constants (struct fields, `extract` and `shuffle` components) are `uint` constants.
- **Locals** have a dense index within their function, so passes can keep per-local data in arrays, and the bytecode
  backend can map them to frame slots.
- **Functions** keep what's rarely needed (name, declaration, entry point, parameters, locals) in a separate
  `IRFunctionInfo`.
- **Ids** are pool indices. The dumper numbers each function's values from 0.

## Instructions

Each op has an entry in a table: its name, operand count, whether it produces a value, whether it's a terminator, and
whether it reads memory, writes memory or has other side effects. The dumper, validator, passes and backends all use
it. The type rules for each op are in the validator.

| Group | Ops | Notes |
|---|---|---|
| Memory | `load`, `store`, `access`, `copy` | `copy dst, src` copies a whole object of the same type |
| Pointers | `offset`, `ptr_to_int`, `int_to_ptr` | reserved |
| Vectors and matrices | `construct`, `extract`, `extract_dynamic`, `shuffle` | `construct float4 %xy, 0.0, 1.0` mixes vectors and scalars. `shuffle a, b, i...` covers swizzles |
| Arithmetic | `add`, `sub`, `mul`, `div`, `rem`, `neg` | component-wise, on scalars, vectors and matrices of the same type. Signed or unsigned by the type |
| Bits | `and`, `or`, `xor`, `not`, `shl`, `shr` | logical on `bool`. `shr` is arithmetic for `int`, logical for `uint` |
| Comparison | `eq`, `ne`, `lt`, `le`, `gt`, `ge` | gives `bool` or a `bool` vector |
| Selection | `select c, a, b` | component-wise |
| Linear algebra | `matmul`, `scale`, `transpose` | `matmul` is matrix × matrix, matrix × vector or vector × matrix. `scale` is vector or matrix × scalar. Reserved until the language has matrices |
| Conversion | `convert`, `bitcast` | |
| Built-ins | `intrinsic` | which one is the sub-op: `min`, `max`, `dot`, `sqrt`, ... A target without one gets a helper function from its emitter |
| Calls | `call f, args...` | |
| Structure | `selection_merge`, `loop_merge` | see below |
| Terminators | `branch`, `branch_if`, `return`, `discard`, `unreachable` | `switch` later |

## Control flow

Blocks form a control flow graph, but it's structured: every `if` and loop is a single-entry region with a known end.

- A block that starts an `if` ends with `selection_merge %merge` before its terminator, naming the block where the
  branches join again.
- A loop header ends with `loop_merge %merge, %continue`: the block after the loop (`break` goes there) and the block
  that ends each iteration and branches back to the header (`continue` goes there).

```
block0:                     // if (c) { a(); } else { b(); }
	selection_merge block3
	branch_if %c, block1, block2
block1:
	call @a
	branch block3
block2:
	call @b
	branch block3
block3:                     // after the if
```

Optimizations see an ordinary CFG. SPIR-V requires these annotations. Text backends print `if` and loops straight from
them, with no structurizer. Bytecode ignores them. The language has no `goto`, so the front end knows every merge point
as it builds the IR; passes have to keep the structure, and the validator checks it.

Blocks are kept in structured order: a block comes after the blocks that dominate it, and a construct's blocks come
before its merge block.

`&&`, `||` and `if` used as an expression become branches writing to a temporary local, or `select` when the right
side has no side effects.

## Functions and memory

- **Parameters** of value types are passed by value. Arrays and structs are passed as a pointer to a whole local that
  nothing else in the call points to; the front end copies a by-value argument into a temporary local. That has the
  same meaning as HLSL's `inout`, MSL's references, SPIR-V's pointer parameters and bytecode references.
- **Arrays and structs are returned** through a pointer to the caller's local, passed instead of a return value.
- **Locals** are listed on their function, with an optional initializer. Without one they start as zero: the language
  zero-initializes everything whose type doesn't specify another initial value.
- **Globals** have an address space, an optional initializer, and for shader inputs and outputs the `ShaderIO` they
  stand for. `private` globals without an initializer start as zero, like locals.
- **Entry points** get a generated wrapper that loads the inputs, calls the function, and stores the outputs. Only
  wrappers touch `input` and `output` globals.

## Semantics and safety

Ops have the targets' raw semantics: out-of-bounds `access` and integer division by zero aren't defined by the IR.
What defines them depends on the target:

- **Shaders:** a safety pass clamps indices that aren't known to be in bounds. The MSL emitter is to guard integer
  division, shifts, signed overflow, float to int conversion and loop termination too (not done yet), which are C++
  undefined behavior there. Elsewhere they only give undefined values, which are accepted, as in WebGL. The decisions and what other
  compilers do are in [Plan/Shaders.md](Plan/Shaders.md) (11).
- **Scripts:** the VM checks bounds, memory accesses and division itself and raises a script error.

## Errors and validation

Every error in the user's code is reported by the front end: the parser, resolver, typer and shader interface pass.
From IR generation on, nothing fails on a program the front end accepted, except that a backend can report a size
limit of its target, like an array too large for D3D11 (Docs/Plan/Shaders.md, 11.4.5). Anything else invalid after
the front end is a compiler bug.

The validator checks a module's structure, operands, dominance and types. Tests and the fuzzers run it after every
step that changes the IR; compiling doesn't.

## SSA

Full SSA is a pass: locals that are only loaded and stored whole, and whose address is never taken, become values, with
**block parameters** where control flow joins: `param` instructions at the start of a block, and arguments as extra
operands on branches. Backends turn them into `OpPhi`, variables assigned in each predecessor, or register moves.
Nothing requires it; the base form is valid for every target.

## Textual form

The dumper prints a module like this. `@` names globals and functions, `%` instruction results and parameters, `$`
locals, numbered per function. Constants are written as their type and value.

```
global @positions   : *constant [3]float2 = {(0.0, 0.5), (0.5, -0.5), (-0.5, -0.5)}
global @VSMain.in0  : *input uint     semantic(vertex_index)
global @VSMain.out0 : *output float4  semantic(position)

function @VSMain(%0: uint): float4
	local $0: *function uint
block0:
	store $0, %0
	%1: uint             = load $0
	%2: *constant float2 = access @positions, %1
	%3: float2           = load %2
	%4: float4           = construct %3, float 0.0, float 1.0
	return %4

function @VSMain.entry(): void [entry vertex]
block0:
	%0: uint   = load @VSMain.in0
	%1: float4 = call @VSMain, %0
	store @VSMain.out0, %1
	return
```
