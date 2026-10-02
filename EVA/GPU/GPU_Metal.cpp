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

static uint32 GetCurrentBackbufferIndex()
{
	return 0;
}

static Pipeline* CreatePipeline(const CreatePipelineOptions&)
{
	return nullptr;
}

static void DestroyPipeline(Pipeline*)
{
}

static void CmdBeginRenderPass(const RenderPassBeginDesc&)
{
}

static void CmdEndRenderPass()
{
}

static void CmdBindPipeline(Pipeline*)
{
}

static void CmdDraw(uint32, uint32)
{
}

static void EndFrame()
{
}

static bool Init(Device& out_device, const InitOptions&)
{
	out_device = Device{
		.backend = Backend::METAL,
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
		.GetCurrentBackbufferIndex = GetCurrentBackbufferIndex,
		.CreatePipeline = CreatePipeline,
		.DestroyPipeline = DestroyPipeline,
		.CmdBeginRenderPass = CmdBeginRenderPass,
		.CmdEndRenderPass = CmdEndRenderPass,
		.CmdBindPipeline = CmdBindPipeline,
		.CmdDraw = CmdDraw,
		.EndFrame = EndFrame,
	};
	return true;
}

BackendDesc backend_desc = {
	.backend = Backend::METAL,
	.Init = Init,
};

}
