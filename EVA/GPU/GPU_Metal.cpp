#include <EVA/GPU/GPU_Metal.hpp>
#include <EVA/PAL/PAL.hpp>
#include <Metal.hpp>
#include <cstdio>
#include <cstdlib>

// Metal through metal-cpp. Objects from new*, alloc and CreateSystemDefaultDevice are owned and released here.

namespace EVA::GPU::Metal
{

static MTL::Device* device = nullptr;
static MTL::CommandQueue* queue = nullptr;
static CA::MetalLayer* layer = nullptr; // the window's, borrowed

// Rendering isn't implemented yet: no backbuffers, and every frame is skipped.
static void Shutdown()
{
	if (queue)
		queue->release();
	if (device)
		device->release();
	device = nullptr;
	queue = nullptr;
	layer = nullptr;
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

static bool InitImpl(const InitOptions& init_options)
{
	if (!init_options.window || !init_options.window->metal_layer)
		return false;
	// Metal reads this when it's loaded, which the window's layer already did, so it can only come from the environment.
	if (init_options.debug && !getenv("MTL_DEBUG_LAYER"))
		fprintf(stderr, "Metal API validation is off; run with MTL_DEBUG_LAYER=1 to turn it on\n");
	device = MTL::CreateSystemDefaultDevice();
	if (!device)
		return false;
	queue = device->newCommandQueue();
	if (!queue)
		return false;

	layer = static_cast<CA::MetalLayer*>(init_options.window->metal_layer);
	layer->setDevice(device);
	layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
	return true;
}

static bool Init(Device& out_device, const InitOptions& init_options)
{
	if (!InitImpl(init_options))
	{
		Shutdown();
		return false;
	}
	out_device = Device{
		.backend = Backend::METAL,
		.backbuffer_format = TextureFormat::BGRA8_UNORM,
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
