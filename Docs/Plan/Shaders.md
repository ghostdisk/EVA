# Shader pipeline plan

The work from shader source to drawing on every target, as numbered items. Each says what's done and what's left.
Design that has landed moves into docs ([IR.md](../IR.md)) and code comments.

**Where we are:** the front end, IR gen and the SPIR-V and D3D11 HLSL backends are done (1-8). Next is the GPU side of
the triangle (9), then the binding model (10). The binding model may force major changes to the language, so work that
would be thrown away by those changes waits until it settles: most of the safety pass (11), the optimizer (12) and the
remaining typer work (2). The index clamps (11.1) landed with the backends, since fxc needs them, and the size limits
(11.4.5) are needed next: valid programs past them make fxc fail.

## Context

- Shaders come from untrusted content (`eva://somegame.com` downloads scripts and shaders), so everything compiles at
  runtime, inside the engine binary. The compiler is the security boundary: its output must be safe for any input,
  since driver compilers aren't hardened.
- Targets: Vulkan 1.0-ish (Android), Metal (iOS, macOS), DX12 (Windows), DX11 (WIP, for keeping ourselves honest).
  GLES / Web is a non-goal.
- DX11 / DX12 tier hardware for now: no bindless.
- Game shaders: speed matters most. Safety is held to what WebGL does, not to WGSL's fully defined semantics (see 11).

## Pipeline

```
lexer -> parser -> resolver -> typer -> shader interface pass -> IR gen -> safety pass -> optimizer -> emitters
(done)   (done)    (done)      (first)  (done, shaders only)     (done)    (clamps)       (later)      (SPIR-V, HLSL)
```

Lexer through the optimizer are shared with scripts, except the shader interface pass. Emitters: SPIR-V and HLSL
first, later MSL for shaders, bytecode for the script VM.

## Summary

| # | Item | Status |
|---|---|---|
| 1 | Built-ins and resolver rules | Done |
| 2 | Typer | First version done, rest deferred |
| 3 | Constants | Done except interning |
| 4 | Shader interface pass | Done |
| 5 | IR and IR gen | Done |
| 6 | Zero-initialization | Done |
| 7 | SPIR-V emitter | Done |
| 8 | HLSL emitter for D3D11 | Done, loops left |
| 9 | Compile result, the triangle on Vulkan and D3D11 | Compile result done, GPU side next |
| 10 | Binding model | Next, the goal of 6-9 |
| 11 | Safety pass and limits | Index clamps done, size limits next, rest deferred |
| 12 | Optimizer | Later |
| 13 | DX12 and Metal backends | Later |

## 1. Built-ins and resolver rules (done)

`Node`, `Type`, `Intrinsic` and `Constant` derive from `Element`, which starts with a 1-byte `ElementKind`. A
`Definition` points to an `Element`, and every resolved identifier is a `REFERENCE` with `Element* target`.

An `Intrinsic` has an `IntrinsicKind` (`SEMANTIC`, `LOCATION`, `ENTRY`; later `dot`, `sin`, `sample`...), a name and an
`argument_scope` (nullptr: arguments resolve normally).

`CompileShader` builds a shader Context that registers `semantic`, `location` and `entry` in the global scope. A script
Context doesn't, so a script using them gets "unknown identifier" without a dedicated check.

Calls resolve the attributes and callee first. If the callee is an `Intrinsic` with an `argument_scope`, the arguments
resolve in a fresh scope under it, so declarations in them don't leak into the context.

- `semantic`'s argument scope is the scope of the `Semantic` enum: an `EnumType` whose values are `ENUM_VALUE` nodes
  (`vertex_index`, `position`, ...). Only those are visible inside `@semantic(...)`, so a field or variable named
  `position` doesn't clash. `Semantic` isn't in the global scope.
- `entry`'s is the `ShaderStage` enum's (`vertex`, `fragment`), the same way.
- `location` has no argument scope, so `@location(COLOR_SLOT)` with a user `const` works.

Where attributes end up in the AST:

| Written as | Attribute is a child of |
|---|---|
| `@entry(vertex) function f()` | the FUNCTION |
| `f(@semantic(vertex_index) id: uint)` | the PARAMETER |
| `f(): @semantic(position) float4` | the RETURN_TYPE node |
| `struct S { @location(0) c: float3; }` | the FIELD |

Left:

1. Decide on attribute shadowing: `const location = 3;` shadows the attribute in that scope. If it matters, look up an
   attribute's head (`Usage::ATTRIBUTE`) in the intrinsic scope first, then fall back to normal lookup.

## 2. Typer (first version done)

Top-down: the expected type is passed down as a hint (literals and initializer lists use it), and the parent checks
the result with `ImplicitCast`.

Done:

- `Node::type`: the value's type, or for a type expression (`DECLARED_TYPE`, `RETURN_TYPE`, an array's `ELEMENT`, a
  constructor's callee) the type it names.
- `ArrayType` (unique per element and length, cached in the Context) and `StructType` (made by the resolver, so a
  struct name is a reference to a type; fields typed and laid out the first time they're needed, with cycle errors).
- NUMBER parsed for the expected type (int, uint, float; hex ints), defaulting to int, or float with a `.` or exponent.
- `TryImplicitCast` / `ImplicitCast` / `ImplicitCoCast`: exact matches only for now.
- Constant expressions (`Script_EvaluateConstant.cpp`) for `const` values, array sizes and `location`: numbers, unary
  and binary arithmetic, vector constructors, initializer lists, references to consts (evaluated on first use, cycles
  are errors), indexing and member access on them.
- Arithmetic `+ - * / %` on matching numeric scalars and vectors, vector constructors (components or a splat),
  indexing arrays (constant indices bounds-checked), struct member access.
- Attributes: `semantic(Semantic)` and `location(constant uint)` on parameters, fields and return types;
  `entry(ShaderStage)` on functions.

Left, deferred until the binding model settles (10), since it may change the language:

1. `bool` and comparisons, `if`.
2. Assignment and `++`/`--`.
3. Calls to user functions (`FunctionType`).
4. Swizzles, matrices, vector-scalar arithmetic.
5. Field and parameter default values, constructing scalars and structs.
6. Lossless implicit conversions.
7. Shader IO as part of `FunctionType`: today two functions with different semantics have the same type.
8. Replace the separate constant evaluator by running constant expressions through the IR.

## 3. Constants (done except interning)

One object per constant, whatever its size: a type plus bytes.

```cpp
struct Constant
{
	Type* type;
	Slice<uint8> bytes; // laid out by the type's size and alignment, padding zeroed
};
```

- The layout is the `size` and `alignment` already on `Type` (`float3` is 12 bytes, 4-aligned). Internal to the
  compiler; buffer layout rules (std140 etc.) apply separately.
- `bool` is stored as a 4-byte 0 or 1.
- The evaluator writes into a byte buffer while walking the type, so `float4(float2(a, b), c, d)` and
  `float4(a, b, c, d)` give the same constant, and a splat `float4(a)` is the same as writing all four.
- Front-end concept, allocated next to `Type`s. The IR references constants and never builds them as trees.

Left:

1. Intern by `(type, bytes)`, so equal constants are the same pointer. Bits, not values: `0.0` and `-0.0` differ, NaNs
   intern.
2. Later: constants referring to strings, functions or objects (scripts) need a side list of relocations.

## 4. Shader interface pass (done)

`Script_ShaderInterface.cpp`. Input: the typed module. Output: a `ShaderInterface` with one `EntryPoint` per
`@entry(...)` function. Each parameter and the return value are flattened to their leaves; each leaf has exactly one
semantic or location and becomes a `ShaderIO` with a path to it.

- Entry points are top-level functions with one `entry` attribute. Semantics and locations on other functions'
  parameters and return values are errors; on struct fields they're allowed anywhere, so structs can be shared.
- Nested structs are allowed. Empty structs, arrays and void can't be inputs or outputs. A `void` return produces no
  outputs. Duplicate locations and semantics per direction are errors.
- Locations are below 32 (no target has more). The device's limits are checked at pipeline creation.
- A vertex entry point has to output `position`.
- Flattening stops at the first error per parameter. With unique locations and semantics and no empty structs, that
  bounds the walk even for exponentially large nested structs.

| Semantic | Stage, direction | Type |
|---|---|---|
| `vertex_index` | vertex in | `uint` |
| `position` | vertex out (clip position), fragment in (fragment coordinate) | `float4` |

Only what the triangle needs; more are added as they're used.

| `location`, stage and direction | Meaning |
|---|---|
| vertex in | vertex attribute |
| vertex out, fragment in | inter-stage value, matched by location |
| fragment out | color target |

The backends pick the concrete form (SPIR-V `Location`, HLSL `TEXCOORDn` / `SV_Targetn` / `ATTRIBn`, MSL
`[[attribute(n)]]` / `[[user(locn)]]` / `[[color(n)]]`).

Left:

1. Reflection, when vertex buffers and multiple render targets arrive: the LOCATION entries for vertex inputs and
   fragment outputs, and VS-out / PS-in matching. A filter over the same `io` list, no new metadata.
2. Resources and bindings, from the binding model (10).

## 5. IR and IR gen (done)

The design is in [IR.md](../IR.md). One IR module per source file, built once. All entry points share it; each entry
point owns its interface globals.

- Ordinary functions are lowered as written. They stay callable by other code.
- `const`: scalars, vectors and matrices become their `Constant*` at each use. Arrays and structs become a `constant`
  global, accessed with `access` + `load` (required for indexing at runtime).
- Per `EntryPoint`: one `input` / `output` global per `ShaderIO`, plus a wrapper that loads the inputs, stores them into
  locals for struct parameters by path, calls the function, loads each output from the returned struct by path, and
  stores the outputs. Only wrappers touch interface globals, so their shape is fixed: loads at the start, stores at the
  end.
- Generated names only in the output: user identifiers never reach the emitters' text (keyword collisions, `main`).
- The fuzzers lower and validate every program that gets through the front end.

How globals map to the shader targets:

| Address space | SPIR-V | HLSL | MSL |
|---|---|---|---|
| `private` | `Private` variable | `static` | lowered to a struct passed down from the entry (no mutable program-scope globals) |
| `constant` | `Private` variable with an initializer, never stored | `static const` | `constant` |
| `input` / `output` | `Input` / `Output` variable + `BuiltIn` / `Location` | fields of the entry's in / out struct | same, with attributes |

Interface globals are never shared between entry points, even with the same semantic. Entry point records (stage, user
name, wrapper function, interface globals) are like `OpEntryPoint`.

The triangle shader (TestApp's):

```
const positions: [3]float2 = { float2(0.0, 0.5), float2(0.5, -0.5), float2(-0.5, -0.5) };

@entry(vertex)
function VSMain(@semantic(vertex_index) vertex_id: uint): @semantic(position) float4
{
	return float4(positions[vertex_id], 0.0, 1.0);
}

@entry(fragment)
function PSMain(): @location(0) float4
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
```

After IR gen (the safety pass would add `%2 = intrinsic min %1, uint 2` before the `access`):

```
global @positions   : *constant [3]float2 = {(0.0, 0.5), (0.5, -0.5), (-0.5, -0.5)}
global @VSMain.in0  : *input uint     semantic(vertex_index)
global @VSMain.out0 : *output float4  semantic(position)
global @PSMain.out0 : *output float4  location(0)

function @VSMain(%0: uint): float4
	local $0: *function uint
block0:
	store $0, %0
	%1: uint             = load $0
	%2: *constant float2 = access @positions, %1
	%3: float2           = load %2
	%4: float4           = construct %3, float 0.0, float 1.0
	return %4

function @PSMain(): float4
block0:
	%0: float4 = construct float 1.0, float 1.0, float 1.0, float 1.0
	return %0

function @VSMain.entry(): void [entry vertex]
block0:
	%0: uint   = load @VSMain.in0
	%1: float4 = call @VSMain, %0
	store @VSMain.out0, %1
	return

function @PSMain.entry(): void [entry fragment]
block0:
	%0: float4 = call @PSMain
	store @PSMain.out0, %0
	return

entry_point vertex   VSMain -> @VSMain.entry  interface { @VSMain.in0, @VSMain.out0 }
entry_point fragment PSMain -> @PSMain.entry  interface { @PSMain.out0 }
```

Left:

1. When assignment lands: build values for assignment into existing memory in a temporary and `copy` it (see
   [TODO.md](../../TODO.md)).

## 6. Zero-initialization (done)

Language semantics, for scripts and shaders alike, not a safety measure: everything starts zeroed unless its type
specifies another initial value.

1. Done: in the IR a local or `private` global without an initializer starts as zero ([IR.md](../IR.md)). SPIR-V
   gives such variables an `OpConstantNull` initializer, HLSL `= (T)0` (`(T[N])0` for arrays). All three existing
   compilers (Tint, naga, ANGLE) zero locals this way.
2. Left: IR gen emits an initializer when the type has a declared initial value, once field default values exist (2).
3. Left: the optimizer (12) may drop the zeroing when every path stores before reading.
4. Later, with compute: workgroup memory has no initializer in HLSL and needs Vulkan's
   `VK_KHR_zero_initialize_workgroup_memory` in SPIR-V. Tint and naga fall back to zero stores by the first invocations
   followed by a barrier.

## 7. SPIR-V emitter (done)

`Script_Backend_SPIRV.cpp`. Per entry point, from its wrapper: everything it reaches, with the wrapper as `main`.

Done:

- SPIR-V 1.0, one module per entry point with a single `OpEntryPoint` listing its `Input` / `Output` variables, with
  `BuiltIn` / `Location` decorations. Integer fragment inputs are `Flat`.
- Constants expanded only here, cached by `(type, bytes)`; elements before the composites using them.
- Shaped like glslang's output, which mobile drivers are tested on: `copy` is a load and store of the whole object
  rather than `OpCopyMemory`.
- Matrix arithmetic and selects column by column, a scalar select condition splat (SPIR-V 1.0 needs as many components
  as the result).
- Signed `%` is `a - b * (a / b)`: truncated like C and HLSL. `OpSRem` with negative operands is undefined in the Vulkan
  environment without `VK_KHR_maintenance8`, and naga saw wrong results on NVIDIA (`-1 % 768 = 255`).
- Only blocks reachable from the entry block; a merge block nothing reaches gets a bare `OpUnreachable`.
- SPIRV-Tools' validator (from the Vulkan SDK) runs on every output in the tests and the fuzzers.

Left:

1. A single constant array of more than 65,532 elements doesn't fit in an instruction (16-bit word count), and the
   emitter panics. The front end has to reject it (11.4.5).
2. No spirv-opt for now; revisit when doing Android seriously (mostly for driver compile time on mobile).

## 8. HLSL emitter for D3D11 (done, loops left)

`Script_Backend_HLSL.cpp`, for fxc (`D3DCompile`, part of Windows) at shader model 5.0. Per entry point, like SPIR-V.

Done:

- Every value a variable assigned once. Pointers, which HLSL doesn't have, are the lvalue text they stand for
  (`l0.m1[v2]`); pointer parameters are `inout`.
- Inputs and outputs are parameters of `main` rather than struct fields: inputs before outputs, sorted by location with
  `SV_*` semantics last. Vertex inputs are `ATTRIBn`, values between stages `TEXCOORDn` (integers `nointerpolation`),
  fragment outputs `SV_Targetn`.
- `if` / `else` printed from `selection_merge`; an empty `else` is left out.
- Floats printed as the shortest text that round trips; NaN, infinity, negative zero and denormals through
  `asfloat(0x...u)`.
- fxc rejects some valid code it can fold to something undefined, so:
  - Integer `/` and `%` by anything but a nonzero constant are printed with a divisor that's never zero,
    `a / (b == 0 ? 1 : b)`. Otherwise `x / 0`, even folded through variables, is error X4010.
  - Indices are clamped (11.1). Otherwise an index that folds to an out of bounds constant is error X3504.
  - Storing to a vector component picked at runtime is a select of the whole vector, like Tint's
    (`v = i == uint4(0u, 1u, 2u, 3u) ? (float4)x : v`); a matrix row is a chain of ifs. fxc can't address them
    (X3500).
- fxc compiles every output in the tests and the fuzzers.

Left:

1. Loops, once the language has them: `[loop]` so fxc doesn't unroll, and `continue` with the continue block's code.
   The emitter panics on `loop_merge` until then.
2. fxc's resource limits, which valid programs can exceed: at most 4,096 vec4 slots of dynamically indexed constant
   data (X4600), and 4,096 temporary registers including indexable arrays. The front end has to reject what doesn't
   fit (11.4.5).
3. A shader model setting, for DX12 (13). `Target` only says HLSL for now.
4. D3D11 matches a pixel shader's inputs to the vertex shader's outputs by register, so a pixel shader that skips one of
   the vertex shader's locations doesn't line up. Sorting by location covers the common cases; the rest needs the
   VS-out / PS-in matching from reflection (4).

## 9. Compile result, the triangle on Vulkan and D3D11 (compile result done, GPU side next)

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

1. `CompileShaderResult` per entry point, for the backend the device uses.
2. Vulkan pipeline creation from the SPIR-V, D3D11 shader creation from fxc's bytecode.
3. TestApp draws the triangle from the compiled shader on both.
4. Engine side, later: compile off the main thread, cache by a hash of the generated code plus the compiler version,
   treat device loss as a normal event.

## 10. Binding model (next, the goal of 6-9)

Uniform buffers, textures and samplers, across Vulkan descriptor sets, D3D12 root signatures, D3D11 slots and Metal
indices. DX11 / DX12 tier: no bindless. Likely globals with a shader-only kind and a binding.

This is the item that may force major changes to the language, so it comes before the deferred work.

1. Design: how resources are declared in the language, how they're grouped and bound, and how that maps to each API.
2. Front end and shader interface pass: resources and their bindings in `ShaderInterface`.
3. IR: resource globals.
4. SPIR-V and HLSL emission, GPU-side creation and binding, a TestApp case using it.
5. Out-of-bounds buffer and texture accesses: decide what's left to the API (see 11.1). D3D defines out-of-bounds
   resource reads as zero and drops writes. Vulkan needs `robustBufferAccess` enabled, and for images
   `robustImageAccess` (1.3 / `VK_EXT_image_robustness`) or `robustImageAccess2` (`VK_EXT_robustness2`).

## 11. Safety pass and limits (deferred, decided)

**Deferred** until the binding model settles. Until it lands, compiled shaders aren't safe to run from untrusted
content.

**Decision: do what WebGL does (ANGLE), not WebGPU (Tint, naga).** These are game shaders and speed comes first.
WebGPU guards everything WGSL defines, including results that are just undefined values. WebGL only guards what can
reach memory it shouldn't, and C++-style undefined behavior in MSL, where LLVM exploits it.

### What existing compilers do

Surveyed in October 2026: Tint (Dawn `12cd231`), naga (wgpu `dd033bf`), ANGLE's translator (`451ec73`).

| Guard | Tint (Chrome WebGPU) | naga (Firefox WebGPU) | ANGLE (WebGL) |
|---|---|---|---|
| Index clamp: function / private / workgroup memory | Always, `min(u32(i), N-1)` | Default on (HLSL: always) | On for WebGL; MSL always |
| Index clamp: buffers | Clamped, except where Vulkan robustness2 or D3D contain it | Separate switch, off when the hardware is robust | Sized arrays clamped; runtime-sized left to robust buffer access |
| Texture loads | Clamped, except on D3D or with `robustImageAccess2` | Clamped on SPIR-V and MSL; raw on HLSL | Left to the driver |
| Texture stores | Never | Never ("all platforms discard") | Never |
| Zero-init locals and private globals | Always | Always | On for WebGL; HLSL and MSL always |
| Zero-init workgroup memory | Default on | Default on | Flag exists, unused |
| Integer `/ %` (÷0, INT_MIN / -1) | All backends, can be disabled | All backends; opt-out for trusted code | MSL only |
| Shifts ≥ bit width | Masked everywhere | Not guarded | MSL only |
| Signed overflow (`+ - *`, negate) | All backends | MSL (negate also HLSL) | MSL only |
| Float → int out of range | Clamped everywhere | Clamped everywhere | MSL only |
| Loop forward progress | 64-bit countdown, all backends | Same countdown, all backends | MSL only |
| Nesting, complexity, recursion limits | Yes | Yes | Yes |

Why they do it:

- **Index clamps** are the core memory-safety guard everywhere. Clamping is skipped only where the API already contains
  the access (D3D, Vulkan robustness features), or where analysis proves the index in range (Tint's integer range
  analysis). ANGLE clamps through float (`int(clamp(float(i), 0.0, float(N-1)))`) because of integer clamp bugs on
  Qualcomm (crbug.com/1217167).
- **Integer division, shifts, signed overflow, float → int:** on SPIR-V and HLSL the result is an undefined value,
  which can't reach memory the index clamps protect, and ANGLE accepts it. In MSL they're C++ undefined behavior that LLVM's
  optimizer exploits, so ANGLE guards them there. Tint and naga guard everywhere mostly because WGSL defines the
  results. None of the three cites a security bug for them, and naga made division guards optional for trusted code.
- **Loop forward progress** is not about GPU hangs: all three leave long but finite loops to the OS watchdog (TDR,
  context loss). Compilers built on LLVM (Metal's, DXC) may assume a loop without side effects terminates and delete
  it. naga documents Metal doing so, and an attacker using that to make the compiler drop the bounds checks naga
  inserted. Tint and naga bound loops with a 64-bit countdown (counting up hung Intel drivers, wgpu#7319); ANGLE puts a
  `volatile` store in each MSL loop it can't prove finite.
- **Where in the pipeline:** none has a real optimizer before its guards. Tint runs robustness early in its pipeline
  and skips clamps only when range analysis proves them unneeded. naga applies every guard while emitting. ANGLE runs
  its passes after light folding and pruning, and the MSL guards during code generation.

### Our work

**11.1 Index clamps (the safety pass proper).**

1. Every dynamic index into an array, vector or matrix in `function`, `private` and `constant` memory gets clamped:
   `min(u32(i), N-1)`, so negative indices land on the last element.
2. Sized arrays in buffers the same way, once buffers exist (10).
3. Runtime-sized arrays and texture accesses: left to the API's robustness (10.5), like ANGLE.
4. Constant indices: already compile errors in the typer.
5. The pass runs on the IR before the optimizer, and the optimizer may remove a clamp it proves unneeded. The index
   clamp in the triangle shader stays: `vertex_index` has no known bound.
6. Watch for the Qualcomm integer `clamp` bug ANGLE works around; use a float clamp there only if we hit it.

**11.2 MSL-only arithmetic guards (with the MSL backend, 13).** In the MSL emitter, not a pass, so pipeline order
doesn't matter:

1. Integer `/` and `%`: replace the divisor with 1 when it's 0, or when it's -1 and the dividend is INT_MIN.
2. Signed `+ - *` and negation computed in unsigned arithmetic.
3. Shift amounts masked with `& (bits - 1)`; signed left shifts done in unsigned arithmetic.
4. Float → int conversions clamped to the representable range first.

SPIR-V and HLSL get none of these.

**11.3 Loop forward progress (once loops exist).**

1. MSL: in every loop not provably finite, a side effect the compiler can't remove (ANGLE: a `volatile bool` store;
   Tint and naga: the 64-bit countdown).
2. DXC (DX12): ANGLE has no DXC backend to follow. Tint and naga bound loops for DXC as well, since it's LLVM-based.
   Decide with the DX12 backend.
3. SPIR-V and fxc: nothing, like ANGLE.

**11.4 Limits.** Each is a compile error, nothing is clamped or rewritten. ANGLE's values are from
`src/compiler/translator/ShaderLang.cpp` and `ParseContext.cpp`.

1. **Tree depth** (expression nesting, statement nesting, array dimensions). ANGLE: 256 each, checked before any other
   pass walks the tree, "to prevent potential stack overflow crashes" in its own passes and in drivers. **Done:**
   `RECURSION_LIMIT = 256` in the parser, resolver and typer, which covers all three since they're one tree.
   **Work:** every later stage that walks the tree or the IR (IR gen, the safety pass, the optimizer, the emitters)
   iterates, or recurses no deeper than the tree. The fuzzers already generate nesting around the limit.
2. **Call depth.** ANGLE: the longest call chain is at most 256 calls; driver compilers inline everything, and GPUs
   have little or no call stack. **Work:** once calls to user functions exist (2.3), compute the longest call chain
   from each entry point and error past 256.
3. **Recursion.** ANGLE, Tint and naga reject it, direct or mutual; no shader target supports it. **Work:** reject call
   cycles in shaders.
4. **Function parameters.** ANGLE: at most 255, the hard limit in SPIR-V and MSL. **Work:** typer error past 255.
5. **Variable sizes.** ANGLE rejects any type of 2 GB or more (driver bugs with 32-bit size overflow), each local,
   global or parameter in private memory of 64 KB or more ("won't fit in the GPU registers anyway", and SPIR-V's
   64K-operand limit), and private memory of 16 MB or more in total. **Work:** typer errors for all three. Today only
   constants are capped (4 MB each, 16 MB in total).
6. **Source size and error count.** Not in ANGLE's list. **Work:** already in [TODO.md](../../TODO.md): cap shader
   source to a few MB, and report only the first N errors.

Not taken from ANGLE:

- **Struct nesting at most 4 levels:** a WebGL 1 spec rule, not a safety guard. The tree depth limit already bounds it.
- **Identifier length (1022 characters):** a limit of the GLSL it outputs. User names never reach our output (5).
- **Macro expansion limits:** no preprocessor.

## 12. Optimizer (later)

Not needed for the first SPIR-V and HLSL: the unoptimized IR is valid to emit.

1. Inline calls into wrappers.
2. Fold `extract(construct(...))`.
3. mem2reg for locals; drop zeroing that's overwritten before being read (6).
4. Constant folding (reusing the typer's evaluator), including `load (access @constant_global, constant index)`.
5. Remove unreferenced functions and globals.
6. Remove safety clamps that are provably in bounds (11.1).

After the safety pass and optimization, the triangle's vertex shader becomes:

```
function @VSMain.entry(): void [entry vertex]
block0:
	%0: uint             = load @VSMain.in0
	%1: uint             = intrinsic min %0, uint 2    // stays: vertex_index has no known bound
	%2: *constant float2 = access @positions, %1
	%3: float2           = load %2
	%4: float4           = construct %3, float 0.0, float 1.0
	store @VSMain.out0, %4
	return
```

## 13. DX12 and Metal backends (later)

1. DX12: the HLSL emitter (8) at a newer shader model, compiled by DXC (`dxcompiler.dll` + `dxil.dll` shipped with the
   engine). Decide on loop bounding for DXC (11.3).
2. MSL: compiled at runtime with `newLibraryWithSource` (async). `private` globals lowered to a struct passed down from
   the entry (5). The arithmetic guards (11.2) and loop forward progress (11.3) in the emitter.
3. Floats printed as in HLSL, with `as_type<float>(...)` for NaN, infinity and denormals.

## Open questions

- Precision: a `half` type or a precision qualifier (`RelaxedPrecision` / `half` / `min16float`). Matters a lot on
  mobile.
- Attribute shadowing (1).
