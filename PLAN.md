# Shader pipeline plan

Temporary. Carried until the triangle shader compiles to SPIR-V and draws on Vulkan, then folded into docs / code comments.

## Context

- Shaders come from untrusted content (`eva://somegame.com` downloads scripts and shaders), so everything compiles at
  runtime, inside the engine binary. The compiler is the security boundary: its output must be safe for any input,
  since driver compilers aren't hardened.
- Targets: Vulkan 1.0-ish (Android), Metal (iOS, macOS), DX12 (Windows), DX11 (WIP, for keeping ourselves honest).
  GLES / Web is a non-goal.
- DX11 / DX12 tier hardware for now: no bindless.

## Pipeline

```
lexer -> parser -> resolver -> typer -> shader interface pass -> IR gen -> safety pass -> optimizer -> emitters
         (done)    (done)                (shaders only)                                                 per entry point
```

Lexer through the optimizer are shared with scripts, except the shader interface pass. Emitters: SPIR-V first, later
HLSL and MSL for shaders, bytecode for the script VM.

## Built-ins and resolver rules

### Definitions (done)

`Node`, `Type`, `Intrinsic` and `Constant` derive from `Element`, which starts with a 1-byte `ElementKind`. A
`Definition` points to an `Element`, and every resolved identifier is a `REFERENCE` with `Element* target`.

An `Intrinsic` has an `IntrinsicKind` (`BUILTIN`, `LOCATION`, `VERTEX`, `FRAGMENT`; later `dot`, `sin`, `sample`...),
a name and an `argument_scope` (nullptr: arguments resolve normally).

### The Context decides what exists (done)

`CompileShader` builds a shader Context that registers `builtin`, `location`, `vertex` and `fragment` in the global
scope. A script Context doesn't, so a script using them gets "unknown identifier" without a dedicated check.

### Resolving calls (done)

Resolve the attributes and callee first. If the callee is an `Intrinsic` with an `argument_scope`, resolve the
arguments in a fresh scope under it, so declarations in them don't leak into the context.

- `builtin`'s argument scope is the scope of the `Builtin` enum: an `EnumType` whose values are `ENUM_VALUE` nodes
  (`vertex_index`, `position`, ...). Only those are visible inside `@builtin(...)`, so a field or variable named
  `position` doesn't clash. `Builtin` isn't in the global scope.
- `location` has no argument scope, so `@location(COLOR_SLOT)` with a user `const` works.

### Where attributes end up in the AST (already the case)

| Written as | Attribute is a child of |
|---|---|
| `@vertex function f()` | the FUNCTION |
| `f(@builtin(vertex_index) id: uint)` | the PARAMETER |
| `f(): @builtin(position) float4` | the RETURN_TYPE node |
| `struct S { @location(0) c: float3; }` | the FIELD |

### Open

- Shadowing: `const location = 3;` shadows the attribute in that scope. If it matters, look up an attribute's head
  (`Usage::ATTRIBUTE`) in the intrinsic scope first, then fall back to normal lookup.

## Typer (first version done)

Top-down: the expected type is passed down as a hint (literals and initializer lists use it), and the parent checks
the result with `ImplicitCast`.

Done:

- `Node::type`: the value's type, or for a type expression (`DECLARED_TYPE`, `RETURN_TYPE`, an array's `ELEMENT`, a
  constructor's callee) the type it names.
- `ArrayType` (unique per element and length, cached in the Context) and `StructType` (made by the resolver, so a
  struct name is a reference to a type; fields typed and laid out the first time they're needed, with cycle errors).
- NUMBER parsed for the expected type (int, uint, float; hex ints), defaulting to int, or float with a `.` or exponent.
- `TryImplicitCast` / `ImplicitCast` / `ImplicitCoCast`: exact matches only for now; lossless conversions later.
- Constant expressions (`Script_EvaluateConstant.cpp`): a subset evaluated at compile time without typing its nodes,
  for `const` values, array sizes and `location`: numbers, unary and binary arithmetic, vector constructors,
  initializer lists, references to consts (evaluated on first use, cycles are errors), indexing and member access on
  them. A separate path until there's IR to run constant expressions through instead; then it can go.
- Arithmetic `+ - * / %` on matching numeric scalars and vectors, vector constructors (components or a splat),
  indexing arrays (constant indices bounds-checked), struct member access.
- Attributes: `builtin(Builtin)` and `location(constant uint)` on parameters, fields and return types; `vertex`,
  `fragment` on functions, without arguments.

Not yet:

- `bool` and comparisons, `if`, assignment and `++`/`--`, calls to user functions (`FunctionType`), swizzles,
  matrices, vector-scalar arithmetic, field and parameter default values, constructing scalars and structs.
- Constants aren't interned yet.
- Shader IO is not part of `FunctionType`: two functions with different builtins have the same type.
- Shaders: reject call cycles (no recursion on any target).
- Limits on declared sizes: array lengths, number of locals and functions, nesting.

## Constants

One object per constant, whatever its size: a type plus bytes.

```cpp
struct Constant
{
	Type* type;
	Slice<uint8> bytes; // laid out by the type's size and alignment, padding zeroed
};
```

- Interned by `(type, bytes)`, so equal constants are the same pointer. Bits, not values: `0.0` and `-0.0` differ,
  NaNs intern.
- The layout is the `size` and `alignment` already on `Type` (`float3` is 12 bytes, 4-aligned). Internal to the
  compiler; buffer layout rules (std140 etc.) apply separately.
- `bool` is stored as a 4-byte 0 or 1.
- The evaluator writes into a byte buffer while walking the type, so `float4(float2(a, b), c, d)`, `float4(a, b, c, d)`
  give the same constant, and a splat `float4(a)` is the same as writing all four.
- Reading an element is an offset into the bytes. Folding `load (access @g, uint 1)` reads at that offset and interns
  the result.
- Front-end concept, allocated next to `Type`s. The IR references constants and never builds them as trees.
- Later: constants referring to strings, functions or objects (scripts) need a side list of relocations.

## Shader interface pass (first version done)

`Script_ShaderInterface.cpp`. Input: the typed module. Output: a `ShaderInterface`, for now one `EntryPoint` per
`@vertex` / `@fragment` function. Resources and bindings go here later.

```cpp
enum class IODirection : uint8 { INPUT, OUTPUT };
enum class IOKind : uint8 { BUILTIN, LOCATION };

struct ShaderIO
{
	IODirection direction;
	IOKind kind;
	Builtin builtin;     // BUILTIN
	uint32 location;     // LOCATION
	Type* type;          // the leaf type
	Slice<uint32> path;  // INPUT: [parameter index, field index, ...]. OUTPUT: [field index, ...] into the return value
	Node* declaration;   // where the attribute was written, for errors
};

struct EntryPoint
{
	Stage stage;
	Node* function;
	Slice<ShaderIO> io;
};
```

### Flattening

Walk each parameter (path `[i]`) and the return value (path `[]`):

```
Flatten(type, attributes, path):
	if type is a struct:
		error if attributes has a builtin or location   // only the leaves carry semantics
		for each field f at index k:
			Flatten(f.type, f.attributes, path + [k])
	else:   // scalar, vector, matrix
		error unless attributes has exactly one builtin or location
		check the builtin against (stage, direction) and type
		append ShaderIO { direction, semantic, type, path }
```

A `void` return produces no outputs. Duplicate locations and builtins per direction are errors.

Done, beyond the above:

- Entry points are top-level functions with one stage attribute. Builtins and locations on other functions'
  parameters and return values are errors; on struct fields they're allowed anywhere, so structs can be shared.
- Nested structs are allowed. Empty structs, arrays and void can't be inputs or outputs.
- Locations are below 32 (no target has more). The device's limits are checked at pipeline creation.
- A vertex entry point has to output `position`.
- Flattening stops at the first error per parameter. With unique locations and builtins and no empty structs, that
  bounds the walk even for exponentially large nested structs.

### Builtins

| Builtin | Stage, direction | Type |
|---|---|---|
| `vertex_index` | vertex in | `uint` |
| `position` | vertex out (clip position), fragment in (fragment coordinate) | `float4` |

Only what the triangle needs; more are added as they're used.

### What `location` means

| Stage, direction | Meaning |
|---|---|
| vertex in | vertex attribute |
| vertex out, fragment in | inter-stage value, matched by location |
| fragment out | color target |

The backends pick the concrete form (SPIR-V `Location`, HLSL `TEXCOORDn` / `SV_Targetn` / `ATTRIBn`, MSL
`[[attribute(n)]]` / `[[user(locn)]]` / `[[color(n)]]`).

### Reflection (later)

Not needed for the triangle. When vertex buffers and multiple render targets arrive: the LOCATION entries for vertex
inputs and fragment outputs, and VS-out / PS-in matching. A filter over the same `io` list, no new metadata.

## IR

One IR module per source file, built once. All entry points share it; each entry point owns its interface globals.

### Module

- **Functions.** Name, `FunctionType`, blocks. `EntryPoint* entry_point` is set only on generated wrappers.
- **Globals.**

  ```cpp
  struct IRGlobal
  {
  	Type* type;
  	GlobalKind kind;       // PRIVATE, CONSTANT, SHADER_INPUT, SHADER_OUTPUT (the last two shader-only)
  	Constant* initializer; // CONSTANT; optional for PRIVATE
  	ShaderIO* io;          // SHADER_INPUT / SHADER_OUTPUT: points into EntryPoint.io
  };
  ```

  | Kind | Script | SPIR-V | HLSL | MSL |
  |---|---|---|---|---|
  | `PRIVATE` | VM data | `Private` variable | `static` | lowered to a struct passed down from the entry (no mutable program-scope globals) |
  | `CONSTANT` | read-only data | `Private` variable with an initializer, never stored | `static const` | `constant` |
  | `SHADER_INPUT` / `SHADER_OUTPUT` | invalid | `Input` / `Output` variable + `BuiltIn` / `Location` | fields of the entry's in / out struct | same, with attributes |

  Interface globals are never shared between entry points, even with the same builtin.
- **Entry point records.** Stage, user name, wrapper function, interface list (like `OpEntryPoint`).

### Instructions

`{ op, Type* result type, operands }`. A per-op table lists the operand kinds (`VALUE`, `LITERAL`, ...), read by the
dumper, the validator and the emitters.

- Values: instruction results, function parameters, `Constant*`s.
- `extract` / `construct` indices are field / element indices whose meaning comes from the composite's type.
  `construct` may mix vectors and scalars (`construct %xy, float 0.0, float 1.0` builds a `float4`), as SPIR-V's
  `OpCompositeConstruct` and HLSL / MSL constructors allow.
- Pointer types exist only in the IR: `*kind T`, where kind is `local` or a global kind. `access` produces one (like
  `OpAccessChain`); `load` / `store` use them.

### Control flow

Basic blocks, structured: every branch names its merge block (like `OpSelectionMerge` / `OpLoopMerge`). The CFG
works for optimizations, and text backends print `if` / loops directly. No gotos in the language, so this is free.

### Locals

Every named variable, parameters included, is a `local` slot accessed with `load` / `store`. No phis. `if` as an
expression writes its result to a temporary slot in each branch and loads it after the merge. A simple mem2reg comes
later. Function-local variables are valid in SPIR-V, HLSL and MSL.

### IR gen

- Ordinary functions are lowered as written. They stay callable by other code.
- `const`: scalars, vectors and matrices become their `Constant*` at each use. Arrays and structs become a `CONSTANT`
  global, accessed with `access` + `load` (required for indexing at runtime).
- Per `EntryPoint`: one `SHADER_INPUT` / `SHADER_OUTPUT` global per `ShaderIO`, plus a wrapper that loads the inputs,
  `construct`s each parameter from them by path, calls the function, `extract`s each output from the return value by
  path, and stores the outputs. Only wrappers touch interface globals, so their shape is fixed: loads at the start,
  stores at the end.
- Generated names only in the output: user identifiers never reach the emitters' text (keyword collisions, `main`).

### Triangle shader

Source (TestApp's, with `@vertex` / `@fragment` added):

```
const positions: [3]float2 = { float2(0.0, 0.5), float2(0.5, -0.5), float2(-0.5, -0.5) };

@vertex
function VSMain(@builtin(vertex_index) vertex_id: uint): @builtin(position) float4
{
	return float4(positions[vertex_id], 0.0, 1.0);
}

@fragment
function PSMain(): @location(0) float4
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
```

After IR gen and the safety pass:

```
global @positions   : [3]float2  CONSTANT       = [3]float2 { (0.0, 0.5), (0.5, -0.5), (-0.5, -0.5) }
global @VSMain.in0  : uint       SHADER_INPUT   builtin vertex_index
global @VSMain.out0 : float4     SHADER_OUTPUT  builtin position
global @PSMain.out0 : float4     SHADER_OUTPUT  location 0

function @VSMain(%vertex_id: uint): float4
@entry:
	%0: *local uint      = local uint
	store %0, %vertex_id
	%1: uint             = load %0
	%2: uint             = min %1, uint 2              // safety pass: clamp the index into [3]float2
	%3: *constant float2 = access @positions, %2
	%4: float2           = load %3
	%5: float4           = construct %4, float 0.0, float 1.0
	return %5

function @PSMain(): float4
@entry:
	%0: float4 = construct float 1.0, float 1.0, float 1.0, float 1.0
	return %0

function @VSMain.entry(): void  [entry vertex]
@entry:
	%0: uint   = load @VSMain.in0
	%1: float4 = call @VSMain(%0)
	store @VSMain.out0, %1
	return

function @PSMain.entry(): void  [entry fragment]
@entry:
	%0: float4 = call @PSMain()
	store @PSMain.out0, %0
	return

entry_point vertex   VSMain -> @VSMain.entry  interface { @VSMain.in0, @VSMain.out0 }
entry_point fragment PSMain -> @PSMain.entry  interface { @PSMain.out0 }
```

After optimization (inline, mem2reg, constant folding, removing unreferenced functions):

```
function @VSMain.entry(): void  [entry vertex]
@entry:
	%0: uint             = load @VSMain.in0
	%1: uint             = min %0, uint 2              // stays: vertex_index has no known bound
	%2: *constant float2 = access @positions, %1
	%3: float2           = load %2
	%4: float4           = construct %3, float 0.0, float 1.0
	store @VSMain.out0, %4
	return

function @PSMain.entry(): void  [entry fragment]
@entry:
	store @PSMain.out0, float4(1.0, 1.0, 1.0, 1.0)
	return
```

## Safety pass

On the IR, before the optimizer, shared by all backends. Tint's robustness transforms and naga's bounds-check
policies are the checklist to compare against.

- Clamp every index into an array that isn't known to be in bounds; constant indices are checked at compile time.
  Out-of-bounds access on private / shared arrays is undefined everywhere, and Metal has no bounds checks at all.
- Zero-initialize every local.
- Defined results for integer division / modulo by zero, `INT_MIN / -1`, shifts by >= the bit width.
- Forward progress: MSL assumes loops terminate. Cap iterations or add an unremovable side effect.
- Later: uniformity analysis for derivatives and barriers.

## Optimizer

Not needed for the first SPIR-V: the unoptimized IR is valid to emit.

- Inline calls into wrappers.
- Fold `extract(construct(...))`.
- mem2reg for locals.
- Constant folding (reusing the typer's evaluator), including `load (access @constant_global, constant index)`.
- Remove unreferenced functions and globals.
- Remove safety clamps that are provably in bounds.

## Emission

Per entry point: walk the call graph from the entry's wrapper and emit only what it reaches (functions, constants,
globals).

### SPIR-V (first)

- SPIR-V 1.0, one module per entry point with a single `OpEntryPoint`. Its interface list holds the entry's
  `SHADER_INPUT` / `SHADER_OUTPUT` globals.
- Interface globals: `Input` / `Output` variables with `BuiltIn` / `Location` decorations.
- `local` slots: `Function` storage variables.
- Constants: expanded only here. A cache from `(type, bytes)` to SPIR-V id, local to the module being emitted:

  ```
  EmitConstant(type, bytes) -> id:
  	if cache has (type, bytes): return it
  	scalar:                    OpConstant type <bits>   (bool: OpConstantTrue / OpConstantFalse)
  	vector/matrix/array/struct:
  		for each element i:    element_ids[i] = EmitConstant(element type of i, bytes + offset of i)
  		OpConstantComposite type element_ids
  	cache and return the id
  ```

  Elements are emitted before the composites using them, so the order is valid without sorting. All-zero constants
  may use `OpConstantNull`.
- Keep the output conventional (shaped like glslang's): mobile drivers are tested on that.
- SPIRV-Tools' validator in tests, fuzzing and debug builds. No spirv-opt for now; revisit when doing Android
  seriously (mostly for driver compile time on mobile).

### Result

```cpp
struct CompiledEntryPoint
{
	Stage stage;
	Atom name;          // the user's name, e.g. VSMain
	Slice<uint8> code;  // SPIR-V, DXBC / DXIL or MSL, depending on the backend
};

struct CompileShaderResult
{
	Slice<CompiledEntryPoint> entry_points;
	Slice<ScriptError*> errors;
};
```

### Later backends

- HLSL: one emitter with a shader model setting. fxc (`D3DCompile`, part of Windows) at SM 5.0 for DX11; DXC
  (`dxcompiler.dll` + `dxil.dll` shipped with the engine) for DX12. `[loop]` on loops so fxc doesn't unroll.
- MSL: compiled at runtime with `newLibraryWithSource` (async).
- Text backends print floats with 9 significant digits; NaN / infinity / denormals through bit-casts
  (`asfloat(0x7fc00000u)`, `as_type<float>(...)`).
- Engine side: compile off the main thread, cache by a hash of the generated code plus the compiler version, treat
  device loss as a normal event.

## Open questions

- Resource binding (uniform buffers, textures, samplers) across Vulkan descriptor sets, D3D12 root signatures,
  D3D11 slots and Metal indices. Likely globals with a shader-only kind and a binding. The next thing to design after
  the triangle.
- Precision: a `half` type or a precision qualifier (`RelaxedPrecision` / `half` / `min16float`). Matters a lot on
  mobile.
- Attribute shadowing (above).

## Order of work

1. ~~Elements, intrinsics, the resolver rule. The resolver's TriangleShader test passes with no errors.~~ Done.
2. ~~Typer: node types, struct / array types, NUMBER parsing, constant evaluator producing `Constant`s.~~ First version done.
3. ~~Shader interface pass producing `EntryPoint`s.~~ First version done; tests and fuzzing next.
4. IR: data structures, op table, dumper, validator, IR gen including wrappers.
5. Safety pass: index clamps (enough for the triangle), then the rest.
6. SPIR-V emitter, checked by the SPIR-V validator in tests.
7. `CompileShaderResult` per entry point; Vulkan pipeline creation in GPU; TestApp draws the triangle.
8. Optimizer passes.
