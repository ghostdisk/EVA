# EVA

EVA is a work-in-progress C++20 game engine and game/editor project. It currently includes an OpenGL renderer, immediate-mode UI, entities and game modes, asset loading, networking, CSG map editing, and a small generated reflection system.

The engine is actively being reorganized from procedural systems toward a more object-oriented model. Expect incomplete APIs, temporary globals, and mixed conventions while that transition is underway.

## Repository layout

- `EVA/` — engine and game source.
- `EVA/Core/` — strings, arenas, allocators, results, serialization, filesystem, and reflection.
- `EVA/Assets/` — runtime asset types and import/loading code.
- `EVA/Entities/` — entity types and `EntityManager`.
- `EVA/GameModes/` — editor and playable game modes.
- `EVA/Editor/` — CSG level editor and reflected editor tools.
- `EVA/Renderer/` — OpenGL setup and drawing API.
- `EVA/Shaders/` — GLSL shaders.
- `Assets/` — project content and editor/runtime maps.
- `EVAGEN/` — Clang-based reflection/code-generation utility.
- `Vendor/` — third-party dependencies.

The main entry points are `EVA/App.cpp`, `EVA/Game.cpp`, and `EVA/Editor/Editor.cpp`.

## Building

```powershell
cmake --preset x64-debug
cmake --build build/x64-debug
```

`EVAGEN_generate` runs before the main target and rewrites `EVA/EVA.gen.cpp`.

`EVA_BASE_DIR` is currently hard-coded in `EVA/Core/Common.hpp` as 'D:/EVA'.

## Core APIs

### String and ZTString

`String` is a non-owning UTF-8 string view.

```cpp
#define STRING_PRINTF_ARGS(str) (int)(str).size, (const char*)(str).data

String name = "example";
String prefix = name.Take(3);
printf("%.*s", STRING_PRINTF_ARGS(name));
```

It stores `data` and `size`, does not own or free memory, and is not guaranteed to end in `\0`. Copies of a `String` copy
only the view. The referenced storage must outlive every view.

`ZTString` extends `String` as a promise that the underlying storage is zero-terminated. It provides `c_str()` and
conversions to C strings.
```cpp
ZTString temporary = Fmt(FrameArena, "entity_%u", eid);
UseImmediately(temporary.c_str());
```

`String::CopyToHeap()` creates a zero-terminated heap copy. Its returned storage currently needs to be freed manually
with `free()`. Prefer arena-backed strings for temporary work.

Both are simple fat pointers and are passed by value. `String` is our preferred type, we rarely rely on the null-terminator,
but when allocating new strings, it's nearly free to null-terminate them, and it often saves a copy when passing it to
OS apis - so a lot of our functions return a `ZTString`, which can be used anywhere a `String` can.


### Arena

An `Arena` is a linear allocator. Allocate quickly, then discard all allocations together with `ArenaReset` or
`ArenaDestroy`; individual allocations cannot be freed.

```cpp
Arena* arena = ArenaCreate();
void* memory = ArenaAllocate(arena, size, alignment);
ZTString label = Fmt(arena, "%s_%d", base, index);
ArenaReset(arena);
ArenaDestroy(arena);
```

`FrameArena` is a global arena that rotates every frame. Memory allocated from it is guarenteed to survive the next 2
frames.

### Result

Fallible operations commonly return `Result`:

```cpp
// EVA/Core/Result.hpp
#define TRY(expr) \
	do { \
		Result res = (expr); \
		if (!res) return res; \
	} while (0)

struct Result {
	// both error's ptr and the data itself are arena allocated.
	// error is arena-allocated so the result fits in 1 register.
	ZTString* error = nullptr;
	...
};
```

```cpp
Result LoadSomething() {
    TRY(LoadDependency());
    if (failed) return Err("failed to load %s", name);
    return Success();
}
```

## Object System and Reflection

`Object` is the root of reflected runtime classes. A reflected class derives, directly or indirectly, from `Object`
and declares `ECLASS_COMMON()`:

```cpp
// Object.hpp:
#define ECLASS_COMMON() \
	static Type* StaticClass(); \
	virtual Type* GetClass() override;
```

```cpp
class MyEntity : public Entity {
public:
    ECLASS_COMMON();
};
```

EVAGEN discovers the class and emits `StaticClass()` and `GetClass()` definitions plus a global `Type`. `Type` exposes:

- `name` — reflected type name.
- `parent_type` — reflected base class.
- `subclasses` — direct reflected subclasses.
- `Instantiate(Allocator)` — default construction through a chosen allocator; absent for abstract types.
- `Type* type = Type::Find(name)` — lookup by reflected name.

Typical dynamic construction:

```cpp
for (Type* type : Tool::StaticClass()->subclasses) {
    Tool* tool = (Tool*)type->Instantiate(Allocator::HeapAllocator);
    // initialize fields not handled by the default constructor
}
```

Reflection instantiation requires a usable default constructor. Initialize external context after construction,
as editor tools do with their `Editor*`.

For generated data serialization, mark a struct or enum `ESERIALIZABLE` and mark serialized fields with `EPROPERTY()`.
These annotations are active while EVAGEN parses the source and compile away in the normal build. Serialization support
is intentionally small and evolving.

## Architecture at a glance

- `Game` owns the active `GameMode` and switches modes through reflection.
- `EntityManager` owns registered entities and their IDs.
- `BasePlayableGameMode` loads compiled maps for play.
- `EditorGameMode` owns `Editor`, which edits CSG operation trees and map entities.
- Editor interactions are reflected `Tool` objects selected through `Editor::SetTool(Tool*)`.
- `.mpe` files are editable maps; `.map` files are compiled runtime maps.
- Rendering uses global draw submission helpers and explicit main/overlay layers.
- Console commands and variables provide much of the current debugging/editor control surface.

## Code style and ownership

The codebase does not yet have one fully consistent style. Preserve the local style when making focused changes, while
steering new systems toward the object-oriented direction already used by game modes, assets, entities, and editor tools.

Current broad conventions:

- Types use `PascalCase`; many free functions use a subsystem prefix such as `Ed`, `CSG`, `UI`.
- Member fields generally use an `m_` prefix; globals generally use `g_`.
- Braces and declaration alignment vary between older and newer files. Avoid formatting unrelated code.
- Prefer explicit ownership. Raw pointers are common and may be owning or observing, so establish which before changing lifetime behavior.
- Match allocation and destruction: `new/delete`, `malloc/free`, arena lifetime, or reflected `Instantiate` plus destructor and allocator `Free`.
- Use `DEFER(...)` for local cleanup where it improves clarity.
- Use `Result`, `TRY`, and `Err` for recoverable failures; assertions are used for programmer errors and internal invariants.
- Keep headers under the `EVA/...` include namespace.

When adding a new reflected class, place it in a header under `EVA/`, add `ECLASS_COMMON()`, ensure its base is publicly
accessible to reflection, and let EVAGEN regenerate the implementation.

