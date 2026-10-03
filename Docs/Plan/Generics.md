# Generics plan

A minimal generics system for built-in types: one mechanism for arrays, vectors and matrices, and the GPU types the
binding model needs (`ConstantBuffer(T)`, `StorageBuffer(T)`; [Bindings.md](Bindings.md), 2.2). A prerequisite of the
binding model. User-defined generics (`struct Foo(T)`) are out of scope, but nothing here should stand in their way.

**Where we are:** steps 1 to 3 of 9 are done: generics, the instance cache and `ResolveName`; `Array`, `Vector` and
`Matrix` (with `float2x2` to `float4x4` named); `type` aliases and constructing through them. Left: defaults, the
texture and buffer generics (step 4, with the binding model). Tests: typer and context unit tests, and the
`Shader/generics` golden case.

Before this, arrays were a special case: `[N]T` parsed to an `ARRAY_TYPE` node that `EvaluateType` handled itself, and
`Context::array_types` cached the types with a linear search. Vectors had their own cache, and matrices existed as
`MatrixType` with nothing making them.

## Summary

| # | Item |
|---|---|
| 1 | Generics: what they are |
| 2 | Instantiation and the instance cache |
| 3 | Syntax and resolving |
| 4 | Typing names: `ResolveName` and its callers |
| 5 | Type aliases: `type` |
| 6 | The built-in generics |
| 7 | What changes in existing code |
| 8 | Later: user generics |
| 9 | Order of work |

## 1. Generics

A **generic** is a built-in `Element`, like `Type` and `Intrinsic`, that a call turns into a type:
`Array(float2, 3)`, `ConstantBuffer(Camera)`. It lives in the context's global scope, so a name resolves to it like
to any built-in.

```cpp
enum class GenericParamKind : uint8
{
	TYPE,      // the argument must evaluate to a type
	CONSTANT,  // the argument must be a constant expression, converted to the parameter's type
};

// An argument, once typed: a type, or a constant of the parameter's type.
struct GenericArg
{
	Type* type = nullptr;         // TYPE
	Constant* constant = nullptr; // CONSTANT
};

struct GenericParam
{
	GenericParamKind kind;
	const char* what;           // in errors: "array size must be a constant"
	PrimitiveType* type;        // CONSTANT: the type the argument is converted to, e.g. uint
	GenericArg default_arg;     // used when the argument is left out; none if both members are nullptr (later)
};

// Makes the type for instance's arguments, which are already typed and converted to the parameters' kinds and types.
// Checks what only the generic knows (a vector's count, a ConstantBuffer's element being plain data), emitting as many
// errors as it finds through typer and returning nullptr. typer is nullptr for internal callers, whose arguments are
// always valid. A type it makes gets instance as its instance; it can also return an existing type. Never called twice
// for the same arguments: the cache (2) is in front of it.
typedef Type* (*InstantiateFn)(Context& context, GenericInstance* instance, Typer* typer);

struct Generic : Element
{
	Atom name;                   // Array, ConstantBuffer...
	Slice<GenericParam> params;
	InstantiateFn instantiate;
	void* user = nullptr;        // for the function: e.g. which buffer kind, or later a struct declaration (later)

	Generic() { kind = ElementKind::GENERIC; }
};
```

- **Name:** `Generic`, and the type a call makes an **instance**. `ElementKind::GENERIC`.
- One `Generic` per built-in, made by `InitContext` like the built-in types and intrinsics.
- Parameters are fixed in number. Variadic generics (function types) aren't needed in the language.
- **Defaults:** trailing parameters can have a default argument, filled in before the cache lookup, so `Texture2D`,
  `Texture2D()` and `Texture2D(float)` are one type. A generic whose parameters all have defaults can be named without
  a call: in a type position, a bare `Texture2D` is the instance with no arguments given (4.1). That's how
  `Texture2D` means `Texture2D(float)` while `Texture2D(int)` stays available.

### Instances remember where they came from

```cpp
struct GenericInstance
{
	Generic* generic;
	Slice<GenericArg> args;
};

struct Type : Element
{
	...
	GenericInstance* instance = nullptr; // set for types made by a generic
};
```

`TypeToString` prints an instance as its generic's name and arguments, `ConstantBuffer(Camera)`, except where a nicer
form exists (`[3]float2`, `float4`). Reflection uses it too (Bindings.md, 5).

## 2. Instantiation and the instance cache

```cpp
// The one type for generic and args, made on first use. nullptr if the function rejects the arguments, with its errors
// emitted through typer. args must already match the parameters (kinds, constant types).
Type* Instantiate(Context& context, Generic* generic, Slice<GenericArg> args, Typer* typer);
```

- **Memoization is the cache's job, not the function's:** a per-context hash table keyed by the generic and its
  arguments. A `TYPE` argument hashes and compares by pointer, since types are unique; a `CONSTANT` argument by its type
  pointer and bytes. On a miss, `Instantiate` calls the generic's function and stores the result with its
  `GenericInstance`.
- **Constants are canonical before lookup**, or `Array(float, 3)` and `Array(float, 3u)` would be two types. The typer
  converts every `CONSTANT` argument to the parameter's type first (6), so the key is always, say, a `uint` 3.
- **Failures aren't cached.** A rejected instantiation reports an error each time it's written.
- **An instance can be an existing type:** `Vector(T, 1)` is `T`. The cache maps the key to `T`, and `T` keeps its own
  `instance` (none for a scalar): only types an instantiate function makes get one.
- **Named instances** (`float4`, `float4x4`) are made through `Instantiate` by `InitContext` and then defined under
  their names, so they carry their `instance` like any other.
- **The table:** `std::unordered_map` with a hash over the generic pointer and the arguments, like the IR's maps, until
  the core library has its own hash map ([TODO.md](../../TODO.md)). Keys and instances are allocated in the context's
  arena.
- **Internal callers** (IR gen, backends, tests) use typed helpers that go through the same cache and assert success:
  `GetArrayType(context, element, length)`, `GetVectorType(context, element, count)`, `GetMatrixType(...)`. Their
  callers don't change.
- **Lifetime:** an instance can refer to a module's own types (`Array(MyStruct, 3)`). For now a module lives as long
  as its context, so the cache can live in the context. Module lifetimes are undecided ([TODO.md](../../TODO.md)).

## 3. Syntax and resolving

A generic is used with call syntax: `Array(float2, 3)`, `ConstantBuffer(Camera)`. The parser needs nothing new: types
are already expressions, and a call is a `CALL` node with a `CALLEE` and `ARGUMENT`s.

- **Resolver:** nothing new either. `Array` resolves to the `Generic` in the global scope like any name, and the
  arguments resolve normally: `Camera` to its struct type, `N` to a const. Shadowing works as for types (a local `Array`
  hides the generic).
- **`[N]T` stays** as the short form for arrays, and so does its `ARRAY_TYPE` node. The typer sends it down the same
  path as `Array(T, N)`, with `context.array_generic`, rather than the parser rewriting it into a call to `Array`, which
  could be shadowed.
- `Array`, `Vector` and `Matrix` are exposed in the language: there's no reason not to, and it gives arrays a spelling
  that composes like the other generics.

## 4. Typing names

Today each caller peels one level of `REFERENCE` and handles its target, which makes every new way of naming something
(an alias, a generic call) a special case in each of them. Instead, one function answers "what does this expression
name?", following references to the end, and the callers list the cases once.

### 4.1 `ResolveName`

```cpp
// What node names, following references, type aliases and generic instantiations to the end: a Type, a Generic, an
// Intrinsic or a declaration Node (FUNCTION, CONST, VARIABLE, PARAMETER, ENUM_VALUE), in *out. *out is nullptr if node
// names nothing, like a literal, arithmetic or a call to a function. Returns false on an error, emitted here or, for an
// unresolved IDENTIFIER, already by the resolver. Doesn't change the tree.
bool ResolveName(Typer& typer, Node* node, Element** out);
```

The `bool` separates "names nothing" (a value, which the caller types) from "failed" (already reported), so callers
don't add a second error to a failed instantiation.

What it follows:

- **`REFERENCE`:** its target. A `TYPE_ALIAS` target is typed on demand (5) and stands for its type; every other
  target is the answer.
- **`CALL` whose callee resolves to a `Generic`:** an instantiation.
  1. Checks the argument count against the parameters, filling in defaults for those left out.
  2. Types each argument by its parameter. `TYPE`: `EvaluateType(argument)`, which completes structs, so instances see
     sizes and fields (an array needs its element's size, `ConstantBuffer(T)` checks `T` is plain data). `CONSTANT`:
     `EvaluateConstant(argument, param.type, ...)`, then converted to `param.type`: an integer constant of another
     integer type converts if the value fits (`int 3` becomes `uint 3`), anything else is an error. Today's array size
     rules move here and into `Array`'s function.
  3. Calls `Instantiate`, which emits the generic's own errors. The answer is the instance.
- **`ARRAY_TYPE`:** the same, with the array generic and the node's `ELEMENT` and `SIZE`.
- **`IDENTIFIER`** (unresolved, already reported) and every other expression: `nullptr`.

**Results are remembered, not rewritten.** An instantiation stores its type in the node's `type`, like `EvaluateType`
does today, and returns it on the next visit without typing the arguments again, so a type expression reached twice
(through an alias, or a struct completed early) costs nothing and reports its errors once. Folded `CONSTANT` arguments
make a second visit cheap anyway. An alias remembers its type on its `TYPE_ALIAS` node. `ResolveName` doesn't change
the tree, because only the caller knows whether the original shape still matters: an error message wanting the
alias's name, a dump, reflection showing `Lights` rather than `ConstantBuffer(LightList)`.

### 4.2 Where it's called

| Caller | Position | What it does with the result |
|---|---|---|
| `EvaluateType` | declared types, return types, array elements, `TYPE` arguments | a `Type` is the answer, stored in the node's `type` as today; a `Generic` whose parameters all have defaults is instantiated with none given (bare `Texture2D`), one without is "'Array' needs arguments"; anything else is "expected a type". The node isn't rewritten: nothing after the typer reads type expressions but their `type`, and rewriting would have to carry every attribute in the dropped arguments along |
| `TypeCall` | a call's callee | below |
| `TypeReference` | a name in a value position | a `Type`, `Generic` or alias: "'x' is a type, not a value"; an `Intrinsic`: "can only be used as an attribute"; nodes as today |
| `TypeAttribute` | an attribute's callee | an `Intrinsic`, or "isn't an attribute" |
| `ConstructorType`, `ReferenceType` (constant evaluator) | the same as `TypeCall` and `TypeReference`, in constant expressions | the same dispatch, so `Vector(float, 3)(...)` and aliases work in consts too |

`TypeCall`:

```cpp
Node* callee = FindChild(node, Usage::CALLEE);
Element* target = ResolveName(typer, callee);
switch (target ? target->kind : ElementKind::NONE)
{
case ElementKind::TYPE: // float3(...), V(...) with type V = float3, Vector(float, 3)(...)
	callee->type = (Type*)target;
	return TypeConstructor(typer, node, (Type*)target);
case ElementKind::GENERIC: // the call is an instantiation, Vector(float, 3) itself, in a value position
	EmitError(typer, "expected a value, got a type");
	return false;
case ElementKind::INTRINSIC: // later: sample(...), length(...); today only attributes
	...
case ElementKind::NODE:
	if (((Node*)target)->node_type == NodeType::FUNCTION)
	{
		EmitError(typer, "calling functions isn't supported yet");
		return false;
	}
	break; // a const or variable: a value, typed below
default:
	break;
}
// The callee is a value: typed for its own errors, then it can't be called.
```

- Constructing an instance falls out of this: once the callee resolves to a `Type`, however it was spelled, it's the
  same as `float3(...)`.
- A call to a generic is always an instantiation, never a construction, so `Texture2D(...)` with arguments is never
  read as constructing a `Texture2D`: a generic with defaults can't be constructed through its bare name. Write the
  instance or an alias, `type F3 = Vector(float, 3); F3(...)`.
- New ways of naming things later (module-qualified names, user generics, enum values through `MEMBER`) go into
  `ResolveName` alone.

## 5. Type aliases: `type`

```
type Lights = ConstantBuffer(LightList);
type Row = [4]float;

struct SceneData { exposure: float; lights: Lights; rows: [4]Row; }
```

A new declaration, `[attributes] type Name = type-expression;`, with a new keyword and node (`TYPE_ALIAS`). `const`
stays what it is, a constant value, with one rule everywhere.

- **Declared ahead,** like structs and functions, in the module and in blocks, so an alias can be used before its
  declaration. `DeclareAhead` declares `TYPE_ALIAS` nodes; their values resolve in the normal pass. That's safe because
  a type expression can't refer to variables: only to types, generics, aliases and consts (array sizes), and consts in
  its value follow their usual rule.
- **Typed on demand,** like consts: a `TypingState` on the node, so `ResolveName` can type an alias the first time it's
  referenced, before its turn, and `type A = B; type B = A;` is "'A' depends on itself". Its value goes through
  `EvaluateType`, and the alias's `type` holds the result.
- **References to an alias** resolve to its type (4.1); chains (`type A = B;`) resolve through `B`'s stored type.
- **Errors:** a value that isn't a type expression ("expected a type"); an alias used as a value ("'Lights' is a type,
  not a value"), in the typer and the constant evaluator.
- The `type` keyword makes `type` unavailable as a name.

## 6. The built-in generics

| Generic | Parameters | Instance | Contexts |
|---|---|---|---|
| `Array` | `element: TYPE`, `length: CONSTANT uint` | `ArrayType`; `[N]T` is the same | all |
| `Vector` | `element: TYPE`, `count: CONSTANT uint` | `VectorType` | all |
| `Matrix` | `element: TYPE`, `columns: CONSTANT uint`, `rows: CONSTANT uint` | `MatrixType` | all |
| `ConstantBuffer` | `element: TYPE` | `GPUBufferType`, kind `CONSTANT` | shader |
| `StorageBuffer` | `element: TYPE` | `GPUBufferType`, kind `STORAGE` | shader |
| `RWStorageBuffer` | `element: TYPE` | `GPUBufferType`, kind `RW_STORAGE`, later | shader |

- **Names for common instances** stay as today: `float4` is `Vector(float, 4)` registered under its own name in the
  global scope, so `float4` and `Vector(float, 4)` are the same type. Matrices get `float2x2` to `float4x4` the same
  way, which covers the matrix prerequisite of Bindings.md (2.3) for types.
- **`Vector` and `Matrix` check their own arguments** and emit errors through the typer:
  - `Vector`: elements `bool`, `int`, `uint` or `float` (`half` later), never vectors or matrices; counts 1 to 4.
    `Vector(T, 1)` is `T` itself, since SPIR-V and MSL have no 1-component vectors.
  - `Matrix`: elements `float` only (`half` later), since SPIR-V matrix columns must be float vectors and MSL only has
    float and half matrices; 2 to 4 columns and rows.
- **`Array`'s checks**, moved from `EvaluateType`: the element is a complete value type (not void, a texture or a
  generic); length at least 1; the array fits in 4 GB (today's `nullptr` from `GetArrayType`).
- **GPU buffer types:**

  ```cpp
  enum class GPUBufferKind : uint8 { CONSTANT, STORAGE, RW_STORAGE };

  struct GPUBufferType : Type
  {
  	GPUBufferKind buffer_kind;
  	Type* element;             // plain data
  };
  ```

  A new `TypeKind::GPU_BUFFER`, opaque (no size, like textures; Bindings.md 3.1). One instantiate function for all
  three, with the kind in `Generic::user`. Its check: the element is plain data (no textures, samplers or buffers
  inside, recursively).
- **Textures are generics over their sample type:** `Texture2D(T)`, `Texture2DArray(T)`, `TextureCube(T)`,
  `Texture3D(T)`, one `TYPE` parameter defaulting to `float`, so a bare `Texture2D` is `Texture2D(float)` (1). `T` is
  `float`, `int` or `uint` (scalars: the read type is the 4-vector of it, like Slang's default `float4`). Instances are
  a `TextureType` with a dimension and sample type, `TypeKind::TEXTURE`. Depth textures and samplers stay plain types.

## 7. What changes in existing code

- **`Context`:** `array_types` and `vector_types` go, replaced by the instance cache; `array_generic`,
  `vector_generic`, `matrix_generic` (and the shader ones) added. `pointer_types` and `function_types` keep their own
  caches: function types are variadic, and pointers, which scripts will use but shaders can't, don't need to be
  generics for now.
- **`GetArrayType` / `GetVectorType`:** become wrappers over `Instantiate` that assert success. `GetArrayType` loses its
  `nullptr` return; its callers in the typer go through `Instantiate`'s errors instead.
- **Lexer, parser, resolver:** the `type` declaration (5).
- **`EvaluateType`:** generic calls and `ARRAY_TYPE` through 4.1, references to aliases through 5.
- **`TypeCall`, `TypeReference`, `ReferenceType`, `ConstructorType`:** the value-position errors of 4.2.
- **`TypeToString`:** instances (1).
- **Tests:** existing expectations don't change, since type expressions keep their shape. New tests: `Array(T, N)` equals `[N]T`, `Vector(float, 4)` equals `float4`,
  argument kind errors (a constant where a type goes and the reverse), `int` sizes converting to `uint`, aliases, alias
  cycles, aliases used as values, generics used as values, nested instances (`Array(Array(float, 2), 3)`), the cache
  returning the same pointer, failures not cached, constructing instances, aliases used before their declaration,
  `Vector(T, 1)` being `T`, and defaults (`Texture2D` equal to `Texture2D(float)`, a bare `Array` an error).
- **Fuzzers:** `FuzzCommon`'s checks expect folded `ARRAY_TYPE` sizes and look types up with `GetArrayType`; they check
  instances against the cache instead. The grammar fuzzer generates `Array(...)`, `Vector(...)` and `type` aliases.

## 8. Later: user generics

Not in this plan; the design leaves room for them.

- `struct Foo(T, N: uint) { ... }` declares a `Generic` whose `user` is the struct's declaration and whose parameters
  come from the declaration. Its instantiate function makes a `StructType` per argument list, typing the fields with
  the parameters bound to the arguments.
- Typing the same declaration once per instance is what needs design: the AST is typed in place, and node types are
  stored on the nodes. Either instances clone the declaration's subtree, or field types are evaluated without
  replacing nodes (an evaluation that returns types and leaves the tree alone). Generic functions have the same
  problem, bigger.
- Instances of user generics are cached the same way, so `Foo(float, 3)` written twice is one type.

## 9. Order of work

1. `Generic`, `GenericInstance`, the cache and `Instantiate`; `ResolveName` and its callers (4); `Array` through it, for
   `Array(T, N)` and `[N]T`. `array_types` goes.
2. `Vector` and `Matrix`; `vector_types` goes; matrix names in the global scope.
3. `type` aliases, and constructing instances through a generic call or an alias (4.2).
4. Default arguments and the texture generics, `ConstantBuffer`, `StorageBuffer` and `GPUBufferType`, with the binding
   model.

Each step lands with its tests and fuzzer updates.
