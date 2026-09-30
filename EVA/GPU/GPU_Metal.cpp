#include <EVA/GPU/GPU_Metal.hpp>

namespace EVA::GPU::Metal
{

// Compile-only backend; native resources will be implemented with metal-cpp.
static void Shutdown()
{
}

static void HandlePALEvent(const PAL::Event&)
{
}

static RenderPass* CreateRenderPass(const RenderPassDesc&)
{
	return nullptr;
}

static void DestroyRenderPass(RenderPass*)
{
}

static Framebuffer* CreateFramebuffer(FramebufferDesc&&)
{
	return nullptr;
}

static void DestroyFramebuffer(Framebuffer*)
{
}

static uint32 GetBackbufferCount()
{
	return 0;
}

static Texture* GetBackbuffer(uint32)
{
	return nullptr;
}

static TextureDesc GetTextureDesc(Texture*)
{
	return {};
}

static bool RecreateSwapchain()
{
	return false;
}

static FrameStatus BeginFrame()
{
	return FrameStatus::SKIP;
}

static Texture* GetCurrentBackbuffer()
{
	return nullptr;
}

static void BeginRenderPass(const RenderPassBeginDesc&)
{
}

static void EndRenderPass()
{
}

static void EndFrame()
{
}

static bool Init(Device& out_device, const InitOptions&)
{
	out_device = Device{
		.Shutdown = Shutdown,
		.HandlePALEvent = HandlePALEvent,
		.CreateRenderPass = CreateRenderPass,
		.DestroyRenderPass = DestroyRenderPass,
		.CreateFramebuffer = CreateFramebuffer,
		.DestroyFramebuffer = DestroyFramebuffer,
		.GetBackbufferCount = GetBackbufferCount,
		.GetBackbuffer = GetBackbuffer,
		.GetTextureDesc = GetTextureDesc,
		.RecreateSwapchain = RecreateSwapchain,
		.BeginFrame = BeginFrame,
		.GetCurrentBackbuffer = GetCurrentBackbuffer,
		.BeginRenderPass = BeginRenderPass,
		.EndRenderPass = EndRenderPass,
		.EndFrame = EndFrame,
	};
	return true;
}

BackendDesc backend_desc = {
	.backend = Backend::METAL,
	.Init = Init,
};

}
