# Vulkan backend

`EVA/GPU/GPU_Vulkan.cpp`, used on Windows and Android, and built wherever CMake finds the Vulkan headers.

- **Vulkan 1.0**, with `VK_KHR_swapchain` and the platform's surface extension. Functions are loaded with volk, so
  nothing links against the loader.
- **Debug:** with `InitOptions::debug`, the validation layer and debug utils are enabled when they're installed, and
  their warnings and errors printed.
- **Device:** a discrete GPU over an integrated one, over anything else.
- **One frame in flight:** a single command buffer and fence. `BeginFrame` waits for the previous frame.
- **Swapchain:** FIFO presentation, one image more than the minimum.

## Notes

### Rotated displays

When a phone is rotated, the display hardware is still in its native orientation (portrait for most phones), so a
landscape frame has to be rotated by 90 degrees on its way to the screen. Vulkan leaves the choice of who does it to the
app, through the swapchain's `preTransform`:

1. **The compositor rotates (current).** `preTransform = IDENTITY`: the app draws in the window's orientation and the
   system rotates the image when it composites it.
2. **The app pre-rotates.** `preTransform = currentTransform`: the app draws an image already rotated into the display's
   native orientation, which the system presents as is.

We do (1). It needs nothing from the rest of the engine: backbuffers are the window's size, and positions, `FragCoord`,
viewports and scissors all mean what they do on every other backend. The cost is on the system's side: where the display
hardware can't rotate the image itself, the compositor does it with an extra GPU pass every frame, costing bandwidth,
power and possibly latency. Android recommends (2) for that reason.

Android reports every present as `VK_SUBOPTIMAL_KHR` while `preTransform` doesn't match the display's current transform,
which with (1) is the whole time the phone is rotated. Suboptimal is therefore only taken as a reason to recreate the
swapchain when the surface's size changed too (`SwapchainMatchesSurface`).

Before the triangle, the backend set `preTransform = currentTransform` without pre-rotating anything. Clearing the screen
looks the same either way, so it went unnoticed until there was geometry, which came out rotated in landscape.

#### Moving to pre-rotation

We'll likely switch to (2) later: we own the shader compiler, so the rotation can be added where the vertex wrapper
already flips Y, invisibly to user code. What it takes:

- **Swapchain:** `preTransform = currentTransform`, with the extent's width and height swapped for 90 and 270 degree
  transforms, and recreated when the transform changes, not only the size (a 180 degree turn keeps the size).
- **Vertex shaders:** rotate the output position in clip space by the transform, after the Y flip. The rotation has to
  apply only when drawing to a backbuffer, not to offscreen targets, while pipelines are shared between both. A push
  constant set per render pass fits this better than a specialization constant, which would need a pipeline per
  rotation.
- **Hide the rotation from the app:**
  - Backbuffer and framebuffer sizes reported in the window's orientation, not the swapchain images'.
  - Viewports and scissors rotated by the backend.
  - `FragCoord` rotated back in the fragment shader wrapper, which needs the framebuffer size.
  - Screen-space derivatives: `ddx` and `ddy` swap, with a sign, under 90 and 270 degrees.
  - Anything that reads a backbuffer back, like screenshots or copies, un-rotated.
