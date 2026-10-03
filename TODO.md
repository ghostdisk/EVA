# TODO

## Script

- `x.5` lexes as `x` followed by the number `.5` rather than a member access, since a `.` followed by a digit always starts a number.
- `a++.b` and `a++++` are accepted: a postfix operator can be followed by more postfix operators and member access.
- Support nested comments
- An attribute's expression swallows a following parenthesis or prefix operator: `@a (x)` parses as the call `a(x)` and `@a -x` as `a - x`. `@a (@b x)`, the example in ParseExpression's comment, parses as `a(@b x)` and then fails on the missing operand.
- A lone `;` (empty statement) is an error, so is a `;` after a statement ending with a block, e.g. `if a {};` (unlike `const`, where it's optional).
- Decide where attributes are allowed, likely declarations only. Today any expression can have them, and on a constant expression, its own or its parts', they move to the folded `CONSTANT` node and are ignored.
- When assignment lands: IR gen has to build values for assignment into existing memory in a temporary and `copy` it, so `s = { s.b, s.a }` reads `s.a` before overwriting it. Add a test for that case, it's easy to break.
- Cap shader source size to a few MB, since shaders come from untrusted content.
- Cap the errors per compile and report only the first N, so a large source can't produce an unbounded list.
- Module lifetimes. For now a module lives as long as its context, so context-level caches can refer to a module's types: today `Context::array_types`, and the generic instance cache that replaces it (`Array(MyStruct, 3)`, Docs/Plan/Generics.md). A long-lived script context with modules loaded and unloaded needs those entries per module, or a module-level cache chained to the context's.
- `std::vector` growth in the compiler (`Parser`, `Resolver` and `Typer` errors, the expression parser's stacks, `Context::array_types`) throws `std::bad_alloc` when out of memory, which ends the process. Decide whether `std::vector` stays allowed; arena-backed lists would make running out a limit error like the rest.

## GPU

- Frame pacing differs per backend. Vulkan waits for the previous frame's fence in `BeginFrame`, so one frame is in flight; Metal is only throttled by `nextDrawable`, so with the layer's 3 drawables the CPU can run 2-3 frames ahead. Harmless while the CPU writes nothing the GPU reads, but the first buffer updated per frame needs a frames-in-flight limit. Pick one count for all backends, and look at latency (D3D11's swap chain has its own queue).
- Metal: `BeginFrame` gets the drawable up front and holds it all frame. Apple recommends getting it just before encoding the pass that renders to it, which matters once there are offscreen passes; that moves the size check that reports `SWAPCHAIN_OUTDATED`.
- Minimized and hidden windows. Vulkan skips frames on a zero surface extent and on `SURFACE_UNAVAILABLE`; Metal keeps calling `nextDrawable`, which can block for up to a second per call while the compositor isn't taking drawables (minimized or fully covered windows), stalling the event loop. Check, and if so have PAL report occlusion (`NSWindowDidChangeOcclusionStateNotification`) so `BeginFrame` returns `SKIP`. Separately, a `SKIP` loop spins at 100% CPU on every backend.
- Metal API validation is off unless `MTL_DEBUG_LAYER=1` is set at launch, since Metal reads it when it loads, so `InitOptions::debug` can't turn it on. Set it (and maybe `MTL_SHADER_VALIDATION=1`) in the debug run configurations. In debug, also create command buffers with `MTL::CommandBufferDescriptor` and `errorOptions = EncoderExecutionStatus`, for errors that say which encoder failed.

## Core

- The atom table stores its strings in `std::string`s keyed by an `std::unordered_map`. Replace both: keep the strings in an arena owned by the table so they never move, and use our own hash map keyed by `StringView`. That removes the `std::string` built on every `GetAtom` lookup, and lets atom strings be returned as views without copying them into the caller's arena (e.g. in `SerializeNode`).
- `GetAtom` isn't thread-safe: the table is a global with no locking. Fine while everything is single threaded, but compiling shaders on worker threads needs the new table to support concurrent lookups and inserts. Its atoms also never go away, so untrusted identifiers grow it for the lifetime of the process.

## Build

- Vendor the Vulkan headers and SPIRV-Tools. Today the Vulkan backend is only built when CMake finds the Vulkan SDK, and the SPIR-V validator the tests and fuzzers use comes from the SDK's `SPIRV-Tools-shared` library, so without the SDK there's no Vulkan backend and SPIR-V output goes unvalidated. SPIRV-Tools generates its tables from SPIRV-Headers' grammar files at build time, so it needs SPIRV-Headers too.
