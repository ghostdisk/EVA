# Binding model plan

Item 10 of [Shaders.md](Shaders.md): how shaders declare resources and vertex inputs, how the compiler lays them out
and reflects them, and how the GPU library creates, uploads and binds them. Numbered items, like Shaders.md. Design
that lands moves into docs ([IR.md](../IR.md), [GPU.md](../GPU/GPU.md)) and code comments.

**Where we are:** nothing of this exists yet. The compiler handles stage inputs and outputs only, and the GPU library
draws a triangle without vertex buffers or resources.

## Summary

| # | Item | Status |
|---|---|---|
| 1 | Concepts | Proposed |
| 2 | Language | Proposed |
| 3 | Layout | Proposed |
| 4 | Mapping to each API | Proposed |
| 5 | Reflection and shader cursors | Proposed |
| 6 | Compiler work | Planned |
| 7 | Vertex buffers | Proposed, by location |
| 8 | GPU library: resources, uploads, bind groups | Planned |
| 9 | Userland binding model | Later, design kept possible |
| 10 | Safety and validation | Proposed |
| 11 | Limits | Reference |
| 12 | Order of work | Proposed |

## Context

- Shaders come from untrusted content and compile at runtime ([Security.md](../Security.md)). Two kinds:
  - **Engine shaders:** ours, shipped with the engine. They get the full binding model.
  - **Userland shaders:** downloaded from a game's server. They get a simplified model (9).
- Targets: Vulkan 1.0 (Android, Windows), Metal (Apple4+ and Mac2), D3D11 (SM 5.0, fxc), D3D12 later (DXC). No bindless
  (Shaders.md, open questions).
- Every backend renders the same pixels for the same input ([GPU.md](../GPU/GPU.md)), but layouts are per target, like
  Slang's: each target gets its own binding numbers and its least bad byte layout (3). Engine code and scripts go
  through reflection and shader cursors (5), never hardcoded offsets or bindings.
- Compiling happens at runtime for the device's backend only, so a compile's reflection describes one target.

## 1. Concepts

Slang's `ParameterBlock` model, mapped to Vulkan descriptor sets, D3D12 register spaces and descriptor tables, D3D11's
flat registers and Metal argument buffers.

- **Bind group:** a set of resources and plain data bound together, the unit of binding, and a self-contained object:
  its constants belong to it. A shader declares up to 4, indices 0 to 3. Four is Vulkan's required
  `maxBoundDescriptorSets` and the common value on Android; nothing in the language depends on it.
- **Plain data:** scalars, vectors, matrices and arrays and structs of them. A group's plain fields are packed into its
  **implicit uniform buffer**, which the group owns, allocated before the group's resources, like Slang's.
- **Opaque types:** textures, samplers and buffers. They take no bytes.
- **Layout units** (Slang's "layout units", `ParameterCategory`): what a type consumes, counted separately per space:
  bytes, binding ranges, and the target's own (Vulkan `binding`s; D3D `b`, `t`, `u` and `s` registers; Metal argument
  buffer `[[id]]`s). A type's size and a field's offset are amounts in all of them at once (3.1).
- **Binding range:** one resource leaf of a group (a texture, sampler or buffer field, or an array of them), or the
  group's implicit uniform buffer. Ranges are numbered depth first, the same way on every target; each says where it
  goes on the target (5).
- **Shader cursor:** a position in a bind group (bytes, binding range, array element) plus the type there, walked with
  `field()` and `element()` and written with `write()`, like Slang's shader cursors (5.3).
- **Layout:** computed per target and per set of layout rules, from a struct type alone. The same struct gives the
  same layout in every shader compiled for the same target.

## 2. Language

### 2.1 Declaring bind groups

```
struct SomeStuff
{
	myTexture: Texture2D;
	mySampler: Sampler;
	someConstantData: float4x4;
	someConstantData2: float4x4;
	storageBuffer: StorageBuffer(Particle);
}

struct SomeOtherStuff
{
	anotherTexture: Texture2D;
	constantBuffer: ConstantBuffer(ObjectData);
}

@bind_group(0) let some_stuff: SomeStuff;
@bind_group(1) let some_other_stuff: SomeOtherStuff;
```

- `let` is the language's declaration statement, `[attributes] let name [: type] [= value];`, allowed wherever a
  declaration is: in function bodies and blocks (locals) and at module level (globals). It replaces today's `name: type`
  expression, and the `:` operator goes away (6.1). A module-level `let` with `@bind_group` is a bind group: it takes a
  struct type and no value, and its value comes from outside the shader.
- `bind_group(n)` is a new attribute intrinsic, like `location`: its argument is a constant `uint` below 4. Two `let`s
  in one module with the same group index are an error.
- No explicit bindings inside a group: the compiler assigns them (3). There's no `register` or `binding` attribute.
- Module-level `let`s are declared ahead, like functions and structs, so functions anywhere in the module can use them.
- Fields are read with member access: `some_stuff.someConstantData`, `some_stuff.myTexture`. A bind group `let` can't be
  assigned, copied whole, passed to a function or compared. Only its fields are values.

### 2.2 Resource types

Built-in types, in the shader context only (the script context doesn't define them, like `semantic` today).

| Type | What | Read with |
|---|---|---|
| `Texture2D(T)`, `Texture2DArray(T)`, `TextureCube(T)`, `Texture3D(T)` | textures read as 4-vectors of `T`: `float` (the default, so bare `Texture2D` is a float texture: unorm, snorm and float formats), `int` or `uint` (integer formats, no filtering) | `sample`, `sample_level` (float only), `load`, `dimensions` |
| `DepthTexture2D`, `DepthTexture2DArray`, `DepthTextureCube` | depth textures, for comparison sampling | `sample_compare` |
| `Sampler`, `ComparisonSampler` | samplers, separate from textures | |
| `ConstantBuffer(T)` | a uniform buffer of its own holding a `T` of plain data, bound separately from the group's implicit one | member access, like a `let` |
| `StorageBuffer(T)` | a read-only buffer of `T`s, its length set by the buffer bound | `buffer[i]`, `length(buffer)` |
| `RWStorageBuffer(T)` | read-write, later (with compute; see 11 for fragment-stage limits) | |

- Textures and samplers are separate everywhere: Vulkan has `SAMPLED_IMAGE` and `SAMPLER`, D3D and Metal always
  separate them. No combined image samplers, since D3D11 has none.
- Textures, `ConstantBuffer(T)` and `StorageBuffer(T)` are generics ([Generics.md](Generics.md)), a prerequisite of
  this plan. Call syntax because types are parsed as expressions, where `StorageBuffer<T>` would parse as comparisons.
- Arrays of resources (`textures: [4]Texture2D`): one binding range; one Vulkan binding of 4 descriptors, 4 D3D
  registers, 4 Metal ids. Indices must be constants for now: SM 5.0 can't index resource arrays dynamically, and
  Vulkan needs optional `shader*ArrayDynamicIndexing` features.
- Plain fields may be nested structs and arrays. A nested struct may hold resources too. **Arrays of structs holding
  resources are an error for now:** Slang splits them into one array per field, so `pairs[1]` isn't contiguous in any
  unit (its documented "splitting of arrays"). Not needed yet.
- `ConstantBuffer(T)` and `StorageBuffer(T)` take plain-data `T` only.

### 2.3 Prerequisites in the front end

From the typer work deferred in Shaders.md (2), these come before or with this item:

1. The `let` statement replacing `:` (6.1).
2. Matrices: `float4x4` and friends as types, plus `mul`. The IR already has them.
3. Calls to built-in functions (`sample`, `length`...): the typer only does vector constructors today. Calls to user
   functions can wait.
4. Stage rules for built-ins: `sample` needs implicit derivatives, so it's fragment only. Vertex shaders use
   `sample_level`. This needs to know which entry points reach a call, which the interface pass can work out.

## 3. Layout

Slang's algorithm ([References/slang/docs/layout.md](../../References/slang/docs/layout.md)), per target:

- Deterministic: a group's layout depends only on its struct type, the target and the layout rules, never on which
  fields a shader uses. Dead fields keep their bindings and bytes, so shaders that declare the same struct agree.
- Different per target, because the targets differ: Vulkan counts one `binding` per resource (and per array), D3D
  counts `b`, `t`, `u` and `s` registers separately and one per array element, Metal counts `[[id]]`s per element.
  Byte layouts differ too (3.3).

### 3.1 Layout units

Every type has a size, and every field an offset relative to its struct, in each of these spaces at once:

| Unit | Counts | Where it's used |
|---|---|---|
| bytes | plain data | the group's uniform buffer, a `ConstantBuffer(T)`'s or `StorageBuffer(T)`'s buffer |
| binding ranges | resource leaves, an array of resources being one; the same on every target | shader cursors (5.3) |
| Vulkan binding | 1 per resource or array of resources | `binding` in the group's set |
| D3D `b`, `t`, `u`, `s` | 1 per resource, N per array, each class separately | registers in the group's space (D3D12) or range (D3D11) |
| Metal id | 1 per resource, N per array | `[[id]]` in the group's argument buffer |

| Type | bytes | ranges | Vulkan | D3D | Metal |
|---|---|---|---|---|---|
| `float4` | 16 | | | | |
| `Texture2D` | | 1 | 1 | 1 `t` | 1 |
| `Sampler` | | 1 | 1 | 1 `s` | 1 |
| `[4]Texture2D` | | 1 | 1 | 4 `t` | 4 |
| `ConstantBuffer(T)` | | 1 | 1 | 1 `b` | 1 |
| `StorageBuffer(T)` | | 1 | 1 | 1 `t` (read-only) | 1 |

### 3.2 Structs and groups

A struct is laid out like Slang lays out any struct: a counter per unit, fields in declaration order. Each field's
offset is the counters' current values (bytes aligned by the byte rules), then its size is added to every counter. A
struct's size is the counters at the end, bytes rounded by the byte rules.

- Offsets are relative to the enclosing struct, so a nested struct's layout doesn't depend on where it's nested, and
  skipping over a struct adds its size in every unit without looking inside. An absolute offset is the sum along the
  path.
- A struct takes a contiguous block of each space (Slang's guarantee): a block of binding ranges, a block of `t`s, a
  block of `s`s and so on, whatever it holds. Arrays of structs holding resources would break this, hence 2.2.

A bind group is laid out like Slang's `ParameterBlock<T>`:

- **Container:** if `T` has plain data, the group's implicit uniform buffer comes first: binding range 0, Vulkan
  `binding` 0, D3D `b0` of the group, Metal `[[id(0)]]`.
- **Element:** `T`'s layout, offset by what the container took (one range, one binding, one `b`, one id). Slang's
  "element var layout".
- **The group itself:** a Vulkan `set`, a D3D12 `space`, a Metal argument buffer; on D3D11, a range of registers per
  class (4).

The SomeStuff and SomeOtherStuff example:

| Range | Field | Vulkan (set, binding) | D3D12 (space, register, table offset) | Metal (buffer, id) |
|---|---|---|---|---|
| 0 | `some_stuff` implicit uniforms | 0, 0 | 0, `b0`, resource 0 | 0, 0 |
| 1 | `some_stuff.myTexture` | 0, 1 | 0, `t0`, resource 1 | 0, 1 |
| 2 | `some_stuff.mySampler` | 0, 2 | 0, `s0`, sampler 0 | 0, 2 |
| 3 | `some_stuff.storageBuffer` | 0, 3 | 0, `t1`, resource 2 | 0, 3 |
| 0 | `some_other_stuff.anotherTexture` | 1, 0 | 1, `t0`, resource 0 | 1, 0 |
| 1 | `some_other_stuff.constantBuffer` | 1, 1 | 1, `b0`, resource 1 | 1, 1 |

`someConstantData` and `someConstantData2` are bytes 0 and 64 of group 0's uniform buffer on every target.

### 3.3 Byte layout rules

Per target, the least bad layout the target and device support. The GPU library picks the rules from the device and
passes them to the compiler (`CompileShaderOptions`), so reflection reflects them.

| Target | Uniform data (implicit buffer, `ConstantBuffer(T)`) | Storage buffers |
|---|---|---|
| Vulkan | scalar (`scalarBlockLayout`), else std430 (`uniformBufferStandardLayout`), else std140 | scalar, else std430 |
| D3D11, D3D12 | HLSL cbuffer packing (fxc, and DXC's default) | `StructuredBuffer` packing (C-like, tight) |
| Metal | Metal's rules (`float3` is 16 bytes, 16-aligned), Slang's `MetalConstantBufferLayoutRules` | packed vectors, Slang's `MetalStructuredBufferLayoutRules` |

- scalar: C-like, every type aligned to its scalar. std430: vectors aligned to their power-of-two size (`float3` is
  16-aligned). std140: std430 plus array strides and struct alignment rounded up to 16. HLSL cbuffer: 16-byte aligned
  arrays and structs, nothing straddles a 16-byte boundary, but later fields can pack into the tail of an array or
  struct (Slang's `HLSLConstantBufferLayoutRulesImpl`).
- Each backend emits its types natively in these rules (`Offset` / `ArrayStride` / `MatrixStride` decorations in SPIR-V,
  plain `cbuffer` and `StructuredBuffer` in HLSL, plain structs in MSL), so no padding or emulation.
- Matrices are column-major in memory on every target (IR.md). HLSL declares them `row_major`, since the emitter's HLSL
  rows are the IR's columns.
- A struct used in uniform data and in a storage buffer has two layouts, reflected separately.
- The language's own layout (`Type::size`, used by constants and script memory) is a separate thing. Scripts write
  bind group data through cursors.

## 4. Mapping to each API

| | Group | Implicit uniform buffer | Resources |
|---|---|---|---|
| **Vulkan** | descriptor `set` = group index | `binding` 0 | `binding` = the range's Vulkan offset |
| **D3D12** (later) | register `space` = group index; a CBV/SRV/UAV table and a sampler table | `b0`, resource table offset 0 | registers from the D3D offsets; table offsets below |
| **D3D11** (SM 5.0, no spaces) | a range of registers per class, after the previous groups' | the group's first `b` | the group's first register of the class + the D3D offset |
| **Metal** | argument buffer at `buffer(g)` | `[[id(0)]]`: a `constant` pointer to the group's buffer | `[[id(n)]]` from the Metal offset |

**D3D12.** Descriptors live in heaps, arrays of descriptors: one shader-visible `CBV_SRV_UAV` heap and one shader-visible
`SAMPLER` heap, bound for the whole frame. A descriptor table is a root parameter holding a handle into a heap, with
ranges declared in the root signature (type, count, base register, space, offset from the table's start). A table can't
mix samplers with the rest, so each group has two:

- **Resource table:** the group's CBVs, SRVs and UAVs, one block in the `CBV_SRV_UAV` heap. A range's offset in it is
  `d3d.cbv + d3d.srv + d3d.uav` of the range's offset: registers count per class, but every class counts in
  declaration order, so their sum is the number of descriptors before it.
- **Sampler table:** the group's samplers, one block in the `SAMPLER` heap. A range's offset is `d3d.sampler`.
- Registers are contiguous per class within the group's space (`t0`, `t1`...), the same as D3D11's within a group, so
  the HLSL is the same for both but for `spaceN`.
- The root signature has two table parameters per group (8 at most), one range per binding range, neighbours merged
  where their registers and offsets both line up. `CmdSetBindGroup` sets the group's two table handles.

**D3D11 is flattened, like Slang for SM 5.0** (`shouldAllocateRegisterSpaceForParameterBlock` is false there): each
group takes a contiguous range of each register class, after the previous groups'. The ranges' starts are computed per
entry point from the groups its module declares and travel with the compiled entry point (5.4). When a group is bound,
the GPU library sets its resources at the bound pipeline's starts, in each stage, so a vertex and a fragment shader
from different modules still work together.

**Metal uses argument buffers, one per group, like Slang's `ParameterBlock`.**

- The group's implicit uniform buffer is the argument buffer's `[[id(0)]]`, a `constant Uniforms*`, so the group is
  self-contained: its constants are in a buffer it owns, referenced from its argument buffer. (Slang puts the plain
  fields in the argument buffer itself, one id each on tier 1, where the argument buffer's layout is the GPU's own; a
  pointer keeps them in Metal's normal byte layout.)
- Other buffers are pointers too: `constant T*` for `ConstantBuffer(T)`, `const device T*` for storage buffers.
- Tier 1 (A8 to A12, including iPhone 8 to XS) is enough, and its limits are above Vulkan 1.0's on Android (11). What
  it rules out, none of which this plan needs: writable textures in argument buffers, GPU-written argument buffers, and
  arrays of or pointers to argument buffers. Argument buffers are filled with `MTLArgumentEncoder`, in shared memory.
  Tier 2's plain C-struct layout (iOS 16, macOS 13) can come later, as another layout (Slang's
  `MetalArgumentBufferTier2` rules).
- Resources referenced through argument buffers need residency: binding a group calls `useResource` (read, vertex and
  fragment stages) for each of its buffers and textures, its uniform buffer included, or `useHeap` once resources come
  from heaps.
- Samplers in argument buffers are created with `supportArgumentBuffers`; iOS allows at most 96 such unique samplers per
  app, so the GPU library deduplicates samplers by their description (10).

**Metal vertex buffers** share the buffer table (31 entries per stage) with the argument buffers (0 to 3), so they
count down from 30 (7).

## 5. Reflection and shader cursors

Produced by the shader interface pass for the target and rules it compiled for, and copied into the result's arena
(the compiler's types live in the intermediate arena, which is freed before `CompileShader` returns). The types go in
`GPUShared.hpp`, next to `CompiledEntryPoint`, since the GPU library consumes them. Two views, like Slang's API:

1. **Type and variable layouts**, for engine code, material editors and scripts: names, types and offsets in every
   unit, as a tree (Slang's `TypeLayoutReflection` / `VariableLayoutReflection`).
2. **Binding ranges**, for the GPU library: the group's resources flattened, each with where it goes (Slang's binding
   ranges and descriptor ranges, what slang-rhi builds descriptor set layouts and root signatures from).

### 5.1 Amounts and type layouts

```cpp
// An amount in every layout unit of one target: a type's size, a field's offset relative to its struct, an array's
// stride. Which member of the union is valid follows from the backend the shader was compiled for.
struct LayoutAmount
{
	uint32 bytes = 0;
	uint32 binding_ranges = 0;  // resource leaves, an array of resources being one; the same on every target
	union
	{
		struct { uint32 binding; } vulkan;
		struct { uint32 cbv, srv, uav, sampler; } d3d; // b, t, u, s registers, one per array element
		struct { uint32 id; } metal;                   // [[id]]s, one per array element
	};
};

enum class ReflectedTypeKind : uint8
{
	SCALAR, VECTOR, MATRIX, ARRAY, STRUCT,
	TEXTURE, SAMPLER, COMPARISON_SAMPLER,
	CONSTANT_BUFFER, STORAGE_BUFFER,
};

struct VarLayout;

// A type laid out for the compile's target and rules.
struct TypeLayout
{
	ReflectedTypeKind kind;
	Atom name;                         // STRUCT
	ScalarKind scalar;                 // SCALAR, VECTOR, MATRIX
	uint32 rows = 1, columns = 1;      // VECTOR, MATRIX
	TextureDimension dimension;        // TEXTURE
	TextureSampleType sample_type;
	uint32 length = 0;                 // ARRAY
	LayoutAmount size;
	uint32 alignment = 1;              // bytes
	LayoutAmount stride;               // ARRAY: per element (an array of resources has no range stride: one range)
	const TypeLayout* element;         // ARRAY. CONSTANT_BUFFER, STORAGE_BUFFER: T, laid out in the buffer's own rules
	Slice<VarLayout> fields;           // STRUCT, in declaration order
};

struct VarLayout
{
	Atom name;
	const TypeLayout* type;
	LayoutAmount offset;               // relative to the enclosing struct
};
```

- Type layouts are made once per compile: a struct used twice is one `TypeLayout`; a struct used with two byte rules
  (in a group and in a storage buffer) is two.
- `ConstantBuffer(T)` and `StorageBuffer(T)` take one range; their `T` has byte offsets from 0 in that buffer.

### 5.2 Bind groups and binding ranges

```cpp
enum class BindingKind : uint8
{
	UNIFORM_BUFFER,                    // the implicit one, or a ConstantBuffer(T)
	STORAGE_BUFFER,
	TEXTURE,
	SAMPLER,
	COMPARISON_SAMPLER,
};

// One resource leaf of a group, or the group's implicit uniform buffer: what it takes and where it goes.
struct BindingRange
{
	BindingKind kind;
	TextureDimension dimension;        // TEXTURE
	TextureSampleType sample_type;
	uint32 count = 1;                  // descriptors, for arrays of resources
	uint32 buffer_size = 0;            // UNIFORM_BUFFER: bytes the shader reads. STORAGE_BUFFER: the element stride
	LayoutAmount offset;               // absolute within the group: the Vulkan binding, the D3D registers (and so the
	                                   // D3D12 table offsets, 4), the Metal id
};

// What a group holds, for the GPU library. Equal layouts are compatible whatever the struct's name; layouts from
// different targets or rules are never compared, since a device has one.
struct BindGroupLayout
{
	Slice<BindingRange> ranges;        // the implicit uniform buffer first when uniform_size > 0, then the resources in
	                                   // declaration order, depth first
	uint32 uniform_size = 0;           // bytes of the implicit uniform buffer
	uint64 hash = 0;                   // of the ranges and size, for caching API objects
};

struct ReflectedBindGroup
{
	uint32 index;                      // 0 to 3
	Atom name;                         // the let's
	const TypeLayout* type;            // the STRUCT
	LayoutAmount element_offset;       // where the struct's offsets start: one range, binding, b and id when there's
	                                   // plain data, else nothing
	BindGroupLayout layout;
};
```

### 5.3 Shader cursors

Slang's shader cursor (https://docs.shader-slang.org/en/latest/shader-cursors.html): a position in a bind group and
the type there. Offsets are consistent, so from a cursor and its type the whole type can be walked, and code writing a
nested struct takes a cursor without caring where the struct is nested.

```cpp
struct ShaderOffset
{
	uint32 bytes = 0;        // into the group's uniform buffer (or a buffer's, for a cursor over a ConstantBuffer(T))
	uint32 range = 0;        // index into the group's layout.ranges
	uint32 array_index = 0;  // element within the range, for arrays of resources
};

struct ShaderCursor
{
	BindGroup* group;
	const TypeLayout* type;
	ShaderOffset offset;

	ShaderCursor field(StringView name) const;
	ShaderCursor field(uint32 index) const;
	ShaderCursor element(uint32 index) const;

	bool write(const void* data, uint32 size); // plain data, exactly type->size.bytes
	bool write(Texture* texture);
	bool write(Sampler* sampler);
	bool write(Buffer* buffer, uint64 offset, uint64 size); // ConstantBuffer(T), StorageBuffer(T)
};

ShaderCursor GetCursor(BindGroup* group); // at the group's struct, offset by element_offset
```

- `field(f)`: `bytes += f.offset.bytes`, `range += f.offset.binding_ranges`, type is the field's.
- `element(i)`: plain arrays, `bytes += i * stride.bytes`; arrays of resources stay in their range,
  `array_index = array_index * length + i`.
- `write`: plain data goes to the uniform buffer at `bytes`; a resource goes to `layout.ranges[range]`, descriptor
  `array_index`, and the range says where: Vulkan `dstBinding` / `dstArrayElement`; D3D12 the resource or sampler table
  offset plus the array index; D3D11 the CPU-side list; Metal the argument encoder's id plus the array index.
- Cursors only carry bytes, a range and an array index, never the target's units: those stay in the compiler and in
  the ranges. The same cursor code runs on every backend.
- Cursors are used by untrusted scripts, so they check everything: unknown fields and out-of-range indices give an
  invalid cursor, and `write` fails on the wrong kind, size or texture dimension and sample type (10).

Example: `Material { albedo: Texture2D; tint: float4; smp: Sampler; }` in
`Stuff { scale: float; material: Material; shadow: Texture2D; lights: [4]Texture2D; }`, as
`@bind_group(0) let stuff: Stuff;`. On D3D12, as `{bytes, ranges, d3d}`, with Vulkan and Metal alongside:

| | Offset (relative) | Size |
|---|---|---|
| `Material.albedo` | `{-, 0, srv 0}`, vk 0, metal 0 | `{-, 1, srv 1}`, vk 1, metal 1 |
| `Material.tint` | `{0}` | `{16}` |
| `Material.smp` | `{-, 1, sampler 0}`, vk 1, metal 1 | `{-, 1, sampler 1}`, vk 1, metal 1 |
| `Material` | | `{16, 2, srv 1, sampler 1}`, vk 2, metal 2 |
| `Stuff.scale` | `{0}` | `{4}` |
| `Stuff.material` | `{16, 0, srv 0, sampler 0}`, vk 0, metal 0 | (`Material`) |
| `Stuff.shadow` | `{-, 2, srv 1}`, vk 2, metal 2 | `{-, 1, srv 1}`, vk 1, metal 1 |
| `Stuff.lights` | `{-, 3, srv 2}`, vk 3, metal 3 | `{-, 1, srv 4}`, vk 1, metal 4 |
| `Stuff` | | `{32, 4, srv 6, sampler 1}`, vk 4, metal 7 |
| group element offset | `{-, 1, cbv 1}`, vk 1, metal 1 | |

| Range | Leaf | D3D12 | Vulkan | Metal |
|---|---|---|---|---|
| 0 | implicit uniforms, 32 bytes | `b0`, resource table 0 | binding 0 | id 0 |
| 1 | `material.albedo` | `t0`, resource table 1 | binding 1 | id 1 |
| 2 | `material.smp` | `s0`, sampler table 0 | binding 2 | id 2 |
| 3 | `shadow` | `t1`, resource table 2 | binding 3 | id 3 |
| 4 | `lights` | `t2`-`t5`, resource table 3-6 | binding 4, 4 descriptors | ids 4-7 |

- `GetCursor(group).field("material")` is `{bytes 16, range 1}`. `.field("tint").write(&c, 16)` writes uniform bytes
  16 to 31; `.field("smp").write(s)` is range 2: sampler table slot 0, Vulkan binding 2, Metal id 2.
- `.field("lights").element(2).write(tex)` is range 4, element 2: resource table slot 3 + 2 = 5 (`t4`), Vulkan
  binding 4 element 2, Metal id 6.
- Byte offsets differ per target, which is why there's reflection at all: in
  `Camera { view: float4x4; proj: float4x4; position: float3; time: float; }`, `time` is byte 140 on D3D and Vulkan
  and 144 on Metal, where `float3` is 16 bytes.
- A `ConstantBuffer(LightList)` field is one range; the buffer bound there is filled with a cursor over the buffer and
  `LightList`'s type layout, bytes only. The group doesn't own that buffer, only its implicit uniforms.

### 5.4 Per compile and per entry point

- `CompileShaderResult`: the backend and layout rules it was compiled for (which `LayoutAmount` union member is valid),
  and `bind_groups`, every group the module declares, used or not.
- `CompiledEntryPoint`:
  - the groups it uses (a mask), so the GPU library only requires those to be bound;
  - for D3D11, each group's first register of each class (4);
  - for vertex shaders, the vertex inputs: location, scalar kind, component count (7);
  - for fragment shaders, the color outputs: location, scalar kind, component count, so the GPU library can check them
    against the render pass's formats.

This is also Shaders.md 4.1 (reflection of locations), which this item covers.

## 6. Compiler work

### 6.1 Front end

1. Lexer and parser: the `let` statement, `[attributes] let name [: type] [= value];`, in blocks and at module level.
   `:` stops being a binary operator (`DECLARATION_PRECEDENCE` goes), so `name: type` is no longer an expression.
   Declarations (`let`, `const`, parameters, fields) parse `name [: type] [= value]` directly instead of parsing an
   expression and reshaping it with `ShapeDeclaration`, which goes away, and so does the attribute shuffling it does.
   `let` makes `VARIABLE` nodes in the parser.
2. Resolver: `ResolveVariable` goes; `VARIABLE` is declared after its type and value, like `CONST` and `PARAMETER`.
   Module-level `let`s are declared ahead. Resource types and `bind_group` go in the shader context's global scope.
3. Typer: `bind_group` (constant `uint` below 4, only on module-level `let`s); resource types; `ConstantBuffer(T)` and
   `StorageBuffer(T)` as type constructors; member access through a `let`; the rules of 2.1 and 2.2; the built-in
   calls of 2.3.
4. Shader interface pass: type layouts and one `ReflectedBindGroup` per `let`, for the target and rules in the compile
   options (3); errors for duplicate groups, resource arrays of structs, and non-plain `T`s. Per entry point: the groups
   it uses, D3D11's starting registers, and the vertex inputs and color outputs.

### 6.2 IR

New types and address spaces, as IR.md anticipates:

- **Handle types** for textures and samplers: opaque values that can only be loaded from their global and passed to
  intrinsics. SPIR-V `OpTypeImage` / `OpTypeSampler` in `UniformConstant`, HLSL `Texture2D` / `SamplerState`, MSL
  `texture2d<float>` / `sampler`.
- **`uniform` address space**, read-only: the implicit uniform buffer and `ConstantBuffer(T)` contents.
- **`storage` address space**, read-only for now. A `StorageBuffer(T)` is a global pointing to a runtime-sized array of
  `T` (a new array type without a length), with an `array_length` intrinsic. SPIR-V 1.0 has no `StorageBuffer` storage
  class: storage buffers are `Uniform` with the `BufferBlock` decoration.
- Globals for these carry their group and binding range (a pointer to the reflected range, like `ShaderIO*` for
  interface globals), which says where the backend puts them.
- Types in `uniform` and `storage` memory carry their type layout, so the backends emit the offsets of 3.3. Loading a
  whole plain struct from them into a local copies member by member, since the two are different types in SPIR-V
  (glslang and DXC do the same).

IR gen splits each `let`, like Slang: one `uniform` global for its plain data (a struct of the plain fields, nested
structs projected the same way), and one global per resource leaf. `some_stuff.someConstantData2` becomes
`access @group0.uniforms, uint 1`; `some_stuff.myTexture` becomes `load @group0.myTexture`. Member access through a
`let` is resolved statically at any depth, so no value of a bind group struct, or of a nested struct holding resources,
ever exists.

Intrinsics: `sample`, `sample_level`, `sample_compare`, `load`, `dimensions`, `array_length`.

`ClampIndices` covers sized arrays in `uniform` and `storage` memory. Runtime-sized arrays and textures are left to
the API's robustness (Shaders.md 10.5 and 11.1).

### 6.3 Backends

All three emit the byte layouts of 3.3 natively and take bindings from the binding ranges.

- **SPIR-V:** `DescriptorSet` and `Binding` decorations; `Offset`, `ArrayStride` and `MatrixStride` decorations from
  the type layouts (scalar layout also needs `VK_EXT_scalar_block_layout` enabled, std430 uniforms
  `VK_KHR_uniform_buffer_standard_layout`); `OpImageSampleImplicitLod` and friends with `OpSampledImage` built from the
  separate image and sampler.
- **HLSL:** `cbuffer`s and `StructuredBuffer`s with `register(bN)`, `register(tN)`, `register(sN)`: for D3D11 the entry
  point's starting registers plus the D3D offsets; for D3D12 the offsets and `spaceN`.
- **MSL:** per group, a struct with `[[id(n)]]` members (`constant Uniforms0* u [[id(0)]]`, then the resources), taken
  by the entry function as `constant Group0& g0 [[buffer(0)]]`. Passed down to the functions that use them, since MSL
  has no global resources (like `private` globals in Shaders.md 13.2.3).

## 7. Vertex buffers

Fixed-function vertex fetch, no vertex pulling. The compiler stays generic: vertex inputs are `@location(n)`, as today.
Fixed formats are an engine convention on top.

**What others do.** WebGPU, sokol-shdc and our current compiler match by location: the shader declares typed inputs at
locations, the pipeline's vertex layout says where each location comes from and in what format, and reflection lists
the shader's locations. Engines with fixed formats add a convention: Filament and Godot fix a location per named
attribute (position 0, normal 1...), bgfx and Unity name channels (`a_position`, `POSITION`, `TEXCOORD0`) and map them
to slots. D3D11 matches by semantic name and index (we emit `ATTRIBn`, so `"ATTRIB"`, n), Vulkan and Metal by location
/ attribute index.

**Shader side.** Unchanged: `@location(n)` on vertex entry point inputs, scalars or vectors of `int`, `uint` or
`float`. Reflection adds each vertex input's location, scalar kind and component count (5.4).

**Pipeline side.**

```cpp
enum class VertexFormat : uint8
{
	FLOAT32, FLOAT32X2, FLOAT32X3, FLOAT32X4,
	UNORM8X4, SNORM8X4, UINT8X4, SINT8X4,
	UNORM16X2, UNORM16X4, SNORM16X2, SNORM16X4, FLOAT16X2, FLOAT16X4,
	UINT32, UINT32X2, UINT32X3, UINT32X4, SINT32, ...
};

struct VertexAttribute { uint32 location; uint32 buffer; uint32 offset; VertexFormat format; };
struct VertexBufferLayout { uint32 stride; bool per_instance; };

struct VertexLayout
{
	Slice<VertexBufferLayout> buffers;
	Slice<VertexAttribute> attributes;
};
```

`CreatePipelineOptions` gets a `VertexLayout`. Each backend builds its own from it: Vulkan's vertex input state,
D3D11's input layout (`"ATTRIB"`, location; created from the vertex shader's bytecode), Metal's `MTLVertexDescriptor`.

**Rules, checked at pipeline creation** (like WebGPU, since shaders are untrusted and D3D11 / Vulkan would otherwise
fail or be undefined):

1. Every vertex input the shader declares has exactly one attribute at its location: a missing one is a Vulkan
   validation error (VUID-07904), a D3D input layout error and a Metal pipeline error. Attributes the shader doesn't
   read are fine. The GPU library never fills a missing attribute itself: an engine that wants defaults binds a default
   buffer with stride 0, as Godot and Filament do.
2. The format's kind matches the input's scalar kind: float, unorm, snorm and half formats feed `float` inputs, uint
   formats `uint`, sint formats `int`. Vulkan leaves a mismatch undefined (VUID-08733) and D3D11 only warns and
   reinterprets the bits, so this check is what keeps backends alike.
3. A format with fewer components than the input is fine: the missing ones read as 0, and 1 for w, on every API. More
   components than the input are dropped. Both are defined the same way everywhere (and in WebGPU).
4. Within the limits (11): locations below 16, at most 16 attributes and 16 buffers. Strides at most 2048 and a
   multiple of 4 (Metal before Apple5 needs 4); offsets a multiple of `min(4, size)`; `offset + size <= stride`, or
   `<= 2048` for stride 0.
5. Formats are WebGPU's set, which every target supports: no 3-component 8 or 16-bit formats (DXGI has none, and
   Vulkan doesn't require them).

Stride is part of the pipeline, since Vulkan 1.0 and Metal need it there; D3D takes it at bind time, from the bound
pipeline. Metal has no stride 0: it becomes `MTLVertexStepFunctionConstant`, as in Dawn, wgpu and MoltenVK.

**Engine formats.** A few fixed layouts (position-only, position-normal-uv, skinned...) and a fixed location per
attribute, Filament-style: position 0, normal 1, tangent 2, color 3, uv0 4, uv1 5, joints 6, weights 7, custom 8+. For
userland shaders those locations come with the engine's declarations (9) as constants:
`@location(ATTRIBUTE_UV0) uv: float2`. Nothing in the compiler or the GPU library knows them.

**Metal buffer indices.** Vertex buffers share the 31-entry buffer table with the groups' argument buffers (0 to 3).
Vertex buffer `i` goes at index `30 - i`, counting down, as in MoltenVK, wgpu and Godot, so with 16 vertex buffers the
two ranges never meet. Dawn and slang-rhi pack vertex buffers after the groups' buffers instead, which makes their
indices depend on the pipeline.

**Out-of-bounds fetch.** Untrusted scripts can make draws whose vertex or index ranges go past a buffer, or index
buffers with large values. D3D11 returns zero; Vulkan is safe with `robustBufferAccess`. Metal has no robust vertex
fetch: the GPU library checks `first_vertex + vertex_count` and instance ranges against the bound buffers at draw time.
Index values it can't check cheaply. Options are validating index buffers on upload (largest index per buffer, checked
against the vertex buffers at draw time), or vertex pulling, as Dawn does for robustness on Metal. Decide with the Metal
work; the first step is the per-buffer largest index.

Limits (minimum guaranteed): 16 attributes and 16 buffers on Vulkan 1.0 and the Android Baseline 2022 profile (stride
up to 2048, offset up to 2047); 32 elements and 32 slots on D3D11; 31 attributes on Metal, and 31 buffers shared with
everything else, no stride limit. WebGPU's defaults are 16 attributes, 8 buffers and a 2048 stride.

## 8. GPU library

What the engine needs to create, upload and bind resources. Kept minimal, like the rest of the library.

### 8.1 Prerequisite: frames in flight

From [TODO.md](../../TODO.md): before the CPU writes anything the GPU reads every frame, pick one frames-in-flight count
for every backend. Per-frame uploads and bind group constants depend on it.

### 8.2 Resources

```cpp
enum BufferUsage : uint32 { BUFFER_VERTEX = 1, BUFFER_INDEX = 2, BUFFER_UNIFORM = 4, BUFFER_STORAGE = 8 };

enum class MemoryKind : uint8
{
	GPU,     // written by uploads, read fast by the GPU
	UPLOAD,  // written by the CPU every frame through a mapping, for dynamic vertices and buffers
};

struct BufferDesc { uint64 size; uint32 usage; MemoryKind memory; };

Buffer* (*CreateBuffer)(const BufferDesc&, Slice<uint8> initial_data);
void* (*MapBuffer)(Buffer*);                 // UPLOAD only, persistently mapped
void (*UploadBuffer)(Buffer*, uint64 offset, Slice<uint8> data);  // GPU memory, through a staging ring

Texture* (*CreateTexture)(const TextureDesc&);  // + usage: sampled, color / depth attachment
void (*UploadTexture)(Texture*, uint32 mip, uint32 layer, Slice<uint8> data);

Sampler* (*CreateSampler)(const SamplerDesc&); // filters, address modes, anisotropy, LOD range, compare op
```

- Uploads are recorded into the frame and run before its first render pass: a staging buffer and
  `vkCmdCopyBuffer(ToImage)` with the layout transitions on Vulkan, a blit encoder on Metal, `UpdateSubresource` on
  D3D11, a copy queue or the direct queue's copies on D3D12.
- Texture formats grow from the four render target formats: `R8`, `RG8`, `RGBA8` and its sRGB form, `RGBA16F`, depth
  formats for sampling, then compressed formats per platform (BC on desktop, ETC2 / ASTC on mobile).

### 8.3 Bind groups

```cpp
BindGroup* (*CreateBindGroup)(const ReflectedBindGroup&);  // empty: filled through a cursor (5.3)
void (*DestroyBindGroup)(BindGroup*);
void (*CmdSetBindGroup)(uint32 index, BindGroup*);
```

- A bind group is made from a group's reflection and filled with `GetCursor(group)` writes. It owns its implicit
  uniform buffer and its API objects: a Vulkan descriptor set, a block in each D3D12 heap, a Metal argument buffer, a
  CPU-side list on D3D11.
- **Resources are set before the group is first bound** and fixed after: tier 1 argument buffers can't change while
  the GPU may read them, and neither can descriptor sets or table blocks in use. Changing a resource means a new group,
  on every backend. Every range must be written before the first bind (10).
- **Constants can change any time.** A group whose constants are written after it was bound keeps one copy of its
  uniform buffer per frame in flight (8.1), allocated on that first write:
  - Vulkan: the copies are in one buffer, binding 0 is `UNIFORM_BUFFER_DYNAMIC`, and the offset picks the frame's copy,
    so one descriptor set serves every frame (4 groups fit the 8 dynamic uniform buffers a pipeline layout may have).
  - D3D12: one resource table block per copy, differing only in the CBV at offset 0.
  - Metal: one argument buffer per copy, differing only in the pointer at `[[id(0)]]`.
  - D3D11: one buffer mapped with `WRITE_DISCARD`; the driver keeps the copies.
  - Constants that change per draw (transforms) belong in a group of their own. Whether that needs a cheaper path than
    a group per object (a per-frame ring) is decided once there's a renderer to measure.
- Pipelines get their layout from their shaders' reflection: no pipeline layout object in the API. The Vulkan backend
  caches `VkDescriptorSetLayout`s by layout hash and `VkPipelineLayout`s by the combination; the D3D12 backend root
  signatures likewise (4). D3D11 sets a group's list per stage at each pipeline's starting registers.
- D3D12 heaps: one shader-visible `CBV_SRV_UAV` heap (up to a million descriptors) and one `SAMPLER` heap (at most 2048)
  for everything; groups allocate blocks in them. Sampler blocks are the scarce part: groups with the same samplers
  could share a block. Decide with the D3D12 backend.
- Metal bind groups own their argument buffer(s) in shared memory, encoded with an `MTLArgumentEncoder` made from the
  layout, and the list of resources to make resident when the group is bound.
- Vulkan descriptor pools: one growing set of pools for groups, freed with their group. Decide the details when
  implementing.

### 8.4 Draws

```cpp
void (*CmdSetVertexBuffers)(uint32 first, Slice<VertexBufferBinding>);  // buffer, offset
void (*CmdSetIndexBuffer)(Buffer*, uint64 offset, IndexFormat);         // UINT16, UINT32
void (*CmdDraw)(uint32 vertex_count, uint32 instance_count, uint32 first_vertex, uint32 first_instance);
void (*CmdDrawIndexed)(uint32 index_count, uint32 instance_count, uint32 first_index, int32 base_vertex,
	uint32 first_instance);
```

`first_instance` isn't free everywhere: D3D11's `SV_InstanceID` doesn't include it, Metal's `[[instance_id]]` does.
Only matters once there's an instance semantic.

## 9. Userland binding model (later)

Not needed now; the design above keeps it possible.

- Groups 0 to 2 belong to the engine (frame globals, the render pass, the draw, or whatever it settles on). Group 3 is
  the userland group.
- Userland shaders don't write `@bind_group`: they declare top-level `let`s, and the compiler collects them, in
  declaration order, into an implicit struct for group 3, like Slang wraps global-scope parameters into a struct and a
  parameter block.

  ```
  let albedo: Texture2D;
  let albedo_sampler: Sampler;
  let tint: float4;
  ```

- Scripts (or a Unity-like material interface) set them through a cursor: `material.field("tint").write(...)`.
- Engine groups are visible to userland shaders through declarations the engine provides, like a prelude, so a userland
  shader can read the view matrix without declaring group 0 itself. This needs a way to share declarations between
  sources (imports or a prelude), which the language doesn't have yet.
- In the compiler: an option for engine or userland mode, the userland group's index, and an error for `@bind_group` in
  userland mode.

## 10. Safety and validation

Shaders and the data bound to them can both come from untrusted content, so the GPU library checks what the drivers
won't, like WebGPU:

- **Cursor writes:** the field exists and indices are in range; plain data is exactly the field's size; a resource
  matches its range's kind, texture dimension and sample type (float, depth, int, uint), comparison samplers only in
  `COMPARISON_SAMPLER` ranges; buffers have the right usage, a `ConstantBuffer(T)` range covers `T`'s size and is within
  the device's maximum, a storage buffer range holds at least one element.
- **First bind of a group:** every range written.
- **Sampler creation:** samplers are deduplicated by description and capped per device, since iOS allows at most 96
  unique samplers usable in argument buffers per app and D3D12's sampler heap holds 2048, and content shouldn't be able
  to exhaust them for the engine.
- **Pipeline creation:** the groups of the vertex and fragment shaders agree where both use one; per-stage resource
  counts within the device's limits (11); vertex inputs against the vertex layout (7); color outputs against the render
  pass formats.
- **Draw:** every group the pipeline uses is bound with an equal layout; every vertex buffer slot the layout uses is
  bound; draw ranges within the vertex buffers, when the backend can't rely on robust vertex fetch (7).
- **In the shader:** sized arrays clamped (6.2). Runtime-sized arrays and texture accesses are left to the API: D3D
  defines out-of-bounds reads as zero; Vulkan needs `robustBufferAccess` (and image robustness for textures), as in
  Shaders.md 10.5.

## 11. Limits

Per stage, from Shaders.md's open questions; the lowest tier is mobile.

| | Vulkan 1.0 required / Android Baseline 2022 | D3D11 FL 11_0 | Metal tier 1, Apple4-5 (Apple2-3) | Metal Apple6+ |
|---|---|---|---|---|
| Bind groups | 4 sets | (flattened) | 4 argument buffers | 4 argument buffers |
| Sampled textures | 16 | 128 | 96 (31) | 1M |
| Samplers | 16 | 16 | 16 | 128 and up |
| Uniform buffers | 12, 16 KB each | 14, 64 KB each | 96 (31) buffers, all kinds | no limit |
| Storage buffers | 4 | SRVs share the 128 | (see above) | |
| Dynamic uniform buffers | 8 per pipeline layout | | | |

- Metal's figures are per stage through argument buffers, from Apple's Metal Feature Set Tables (May 2026). On tier 1,
  resources in argument buffers and resources set directly count against the same limits. Tier 1 is above Vulkan 1.0's
  Android minimums everywhere, with samplers equal at 16, so Android stays the floor.
- Limits are checked against the device at pipeline creation (10), like locations today. The compiler only errors where
  it can't emit, like a D3D11 register index past the target's table (Shaders.md 11.4.5).
- Uniform data per group above 16 KB fails on part of Android; above 64 KB on D3D11 everywhere.
- Writable storage buffers in fragment shaders need `fragmentStoresAndAtomics` on Vulkan (optional) and share D3D11's
  8 output slots with render targets, hence later.

## 12. Order of work

1. Front-end prerequisites (2.3): generics ([Generics.md](Generics.md)), the `let` statement replacing `:`, matrices
   (through generics), built-in calls.
2. `@bind_group` with plain data only: layouts, reflection, cursors, IR `uniform` globals, all three backends, GPU
   buffers, bind groups with their implicit uniform buffer, TestApp drawing with a uniform (a transform).
3. Vertex buffers (7): layouts in pipeline creation, validation, TestApp drawing a mesh from a vertex and index buffer.
4. Textures and samplers: resource types, `sample` and friends, texture uploads, TestApp drawing a textured mesh.
5. `ConstantBuffer(T)`, `StorageBuffer(T)`.
6. Later: D3D12, userland model (9), `RWStorageBuffer`, storage textures, Metal tier 2 argument buffer layout, push
   constants.

## Open questions

Undecided; to revisit once generics ([Generics.md](Generics.md)) are in.

1. **`let` semantics** (the syntax is decided, 2.1): are `let`s mutable once assignment lands, or immutable with another
   keyword for mutable ones? Is a type required when there's a value, or inferred? And what's a module-level `let`
   without `@bind_group`: a `private` global, or an error for now?
2. **The cursor API exposed to engine code and scripts:** the shape in 5.3 is Slang's; names, error reporting (invalid
   cursors or failed writes) and the script-side binding are still open.
3. **Per-draw constants:** a group per object, or a cheaper path (8.3).

Decided since: resource types are generics with call syntax, and integer textures are `Texture2D(int)` /
`Texture2D(uint)`, with a bare `Texture2D` meaning `Texture2D(float)` (2.2).
