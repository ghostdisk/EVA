# GPU

The GPU library (EVA/GPU) is a low-level RHI: one API for render passes, pipelines and draws over each platform's
graphics API. It's kept minimal, growing as the engine needs more than a triangle.

## Backends

| Backend | Platforms | Shaders | Notes |
|---|---|---|---|
| Vulkan | Windows, Android | SPIR-V | [Backend_Vulkan.md](Backend_Vulkan.md) |
| D3D11 | Windows | HLSL, compiled with fxc when the pipeline is created | |
| Metal | macOS | MSL, compiled by Metal when the pipeline is created | |

`GPU::Init` picks the backend: the preferred one if it's compiled in, otherwise the first available, in the order Metal,
Vulkan, D3D11. The backend fills in `GPU::device`, a table of function pointers, and nothing else in the library knows
which one is running.

Shaders are written in the scripting language and compiled by EVA/Script for `device.backend`
([Plan/Shaders.md](../Plan/Shaders.md)). What the two libraries share, the backend enum and compiled entry points, is in
`EVA/Core/GPUShared.hpp`.

## Conventions

Every backend renders the same pixels for the same input. Where the APIs differ, the backends and shader compiler make
them agree, and user code never sees the difference:

- **Clip space is D3D's and Metal's:** Y points up. For Vulkan, whose Y points down, the vertex shader wrapper negates the
  Y of the position it outputs.
- **Clockwise on screen is the front face.**
- **Backbuffers are in the window's current orientation,** with its width and height. On rotated mobile displays this
  takes extra work on some APIs, see the Vulkan notes.

## Frame loop

Commands are recorded between `BeginFrame` and `EndFrame`, by the device functions starting with `Cmd`.

`BeginFrame` returns:

- `OK`: record the frame.
- `SKIP`: there's nothing to draw to right now, like when the window is minimized or the Android surface is gone. Poll
  events and try again.
- `SWAPCHAIN_OUTDATED`: the backbuffers no longer match the window. Destroy everything that references them
  (framebuffers), call `RecreateSwapchain`, and create them again.

The app passes every PAL event to `device.HandlePALEvent`, which is how the backend learns about resizes and surfaces
appearing and disappearing.

A render pass declares, for each attachment, the image state before, during and after the pass, and the backend issues
the transitions. Backbuffers start `UNDEFINED` and end in `PRESENT`.
