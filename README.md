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
- `EVA/GFX/` — OpenGL setup and drawing API.
- `EVA/ShaderFiles/` — GLSL shaders.
- `Assets/` — project content and editor/runtime maps.
- `EVAGEN/` — libclanglang-based reflection/code-generation utility.
- `Vendor/` — third-party dependencies.

The main entry points are `EVA/App.cpp`, `EVA/Game.cpp`, and `EVA/Editor/Editor.cpp`.

## Rules


## Building

```powershell
cmake --preset x64-debug
cmake --build build/x64-debug
```

`EVAGEN_generate` runs before the main target and rewrites `EVA/EVA.gen.cpp`.

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
ZTString temporary = FrameArena->Fmt("entity_%u", eid);
UseImmediately(temporary.c_str());
```

`String::CopyToHeap()` creates a zero-terminated heap copy. Its returned storage currently needs to be freed manually
with `free()`. Prefer arena-backed strings for temporary work.

Both are simple fat pointers and are passed by value. `String` is our preferred type, we rarely rely on the null-terminator,
but when allocating new strings, it's nearly free to null-terminate them, and it often saves a copy when passing it to
OS apis - so a lot of our functions return a `ZTString`, which can be used anywhere a `String` can.


### Arena

An `Arena` is a linear allocator. Allocate quickly, then discard all allocations together with `Reset` or
`Destroy`; individual allocations cannot be freed.

```cpp
Arena* arena = Arena::Create();
void* memory = arena->Allocate(size, alignment);
ZTString label = arena->Fmt("%s_%d", base, index);
arena->Reset();
arena->Destroy();
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

## Reflection

There's a custom clang-based reflection system. You can read EVAGEN/evagen.cpp to more or less see how it works if needed.

## Rules

1. Don't add CRT/libc/STL includes. Don't add Windows.h includes. Don't add any includes in .hpp files unless you have
good reason to, prefer always forward declaring. We take compile times very seriously, to the point where we're
forward declaring common libc operations in order to avoid project-wide includes to common stl/libc headers which otherwise

2. Don't use the STL for anything, ever, unless prompted. No <algoritm>, no <vector>, no <unordered_map>

3. Don't use libc either, unless there's already precedent in the codebase - e.g. we use math.h, FILE, and printf-based APIs
currently - if you're about to use something for libc, grep first. If it's not yet used, it's not allowed.

## Object System and Reflection

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
