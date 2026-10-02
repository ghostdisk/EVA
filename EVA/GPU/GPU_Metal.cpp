#include <EVA/GPU/GPU_Metal.hpp>
#include <EVA/PAL/PAL.hpp>
#include <EVA/Core/Panic.hpp>
#include <Metal.hpp>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define MTL_ASSERT(expr)                                                                                                \
	do                                                                                                                  \
	{                                                                                                                   \
		if (!(expr))                                                                                                    \
			Panic("%s failed at %s:%d", #expr, __FILE__, __LINE__);                                                     \
	} while (0)

// Metal through metal-cpp. Objects from new*, alloc and CreateSystemDefaultDevice are owned and released here; the rest
// are autoreleased, so code getting them runs inside an autorelease pool: the frame's from BeginFrame to EndFrame, or one
// of its own.
//
// Metal tracks hazards between passes itself, so the attachments' image states need no barriers.

namespace EVA::GPU::Metal
{

struct MetalTexture
{
	MTL::Texture* texture = nullptr;
	TextureDesc desc;
};

struct MetalRenderPass
{
	std::vector<AttachmentDesc> attachments;
};

struct MetalPipeline
{
	MTL::RenderPipelineState* state = nullptr;
};

// Keeps the textures rather than their MTL::Textures: the backbuffer's changes every frame.
struct MetalFramebuffer
{
	std::vector<MetalTexture*> attachments;
	uint32 width = 0;
	uint32 height = 0;
};

static MTL::Device* device = nullptr;
static MTL::CommandQueue* queue = nullptr;
static CA::MetalLayer* layer = nullptr; // the window's, borrowed
// CAMetalLayer hands out a different drawable each frame, so there's one backbuffer, whose texture is the current
// drawable's between BeginFrame and EndFrame.
static MetalTexture backbuffer;
static bool swapchain_dirty = false;
static uint32 window_width = 0; // in pixels, from the last resize
static uint32 window_height = 0;

// The current frame's, autoreleased into frame_pool.
static NS::AutoreleasePool* frame_pool = nullptr;
static CA::MetalDrawable* drawable = nullptr;
static MTL::CommandBuffer* command_buffer = nullptr;
static MTL::RenderCommandEncoder* encoder = nullptr;
static MTL::CommandBuffer* last_command_buffer = nullptr; // retained, waited for on shutdown

static MetalTexture* ToImpl(Texture* texture)
{
	return reinterpret_cast<MetalTexture*>(texture);
}
static MetalRenderPass* ToImpl(RenderPass* render_pass)
{
	return reinterpret_cast<MetalRenderPass*>(render_pass);
}
static MetalFramebuffer* ToImpl(Framebuffer* framebuffer)
{
	return reinterpret_cast<MetalFramebuffer*>(framebuffer);
}
static MetalPipeline* ToImpl(Pipeline* pipeline)
{
	return reinterpret_cast<MetalPipeline*>(pipeline);
}

static MTL::PixelFormat ToMTLPixelFormat(TextureFormat format)
{
	switch (format)
	{
	case TextureFormat::RGBA8_UNORM:
		return MTL::PixelFormatRGBA8Unorm;
	case TextureFormat::BGRA8_UNORM:
		return MTL::PixelFormatBGRA8Unorm;
	case TextureFormat::D24_UNORM_S8_UINT:
		// Apple GPUs have no 24-bit depth, so it's the nearest format they all have.
		return MTL::PixelFormatDepth32Float_Stencil8;
	}
	return MTL::PixelFormatInvalid;
}

static MTL::LoadAction ToMTLLoadAction(AttachmentLoadOp op)
{
	switch (op)
	{
	case AttachmentLoadOp::LOAD:
		return MTL::LoadActionLoad;
	case AttachmentLoadOp::CLEAR:
		return MTL::LoadActionClear;
	case AttachmentLoadOp::DONT_CARE:
		return MTL::LoadActionDontCare;
	}
	return MTL::LoadActionDontCare;
}

static MTL::StoreAction ToMTLStoreAction(AttachmentStoreOp op)
{
	return op == AttachmentStoreOp::STORE ? MTL::StoreActionStore : MTL::StoreActionDontCare;
}

static uint32 GetBackbufferCount()
{
	return layer ? 1 : 0;
}

static Texture* GetBackbuffer(uint32 index)
{
	return index < GetBackbufferCount() ? reinterpret_cast<Texture*>(&backbuffer) : nullptr;
}

static TextureDesc GetTextureDesc(Texture* texture)
{
	return texture ? ToImpl(texture)->desc : TextureDesc{};
}

static uint32 GetCurrentBackbufferIndex()
{
	return 0;
}

static bool RecreateSwapchain()
{
	if (!layer || !window_width || !window_height)
		return false;
	layer->setDrawableSize(CGSizeMake(window_width, window_height));
	backbuffer.desc.width = window_width;
	backbuffer.desc.height = window_height;
	swapchain_dirty = false;
	return true;
}

static void EndFramePool()
{
	frame_pool->release();
	frame_pool = nullptr;
	drawable = nullptr;
	command_buffer = nullptr;
	backbuffer.texture = nullptr;
}

static FrameStatus BeginFrame()
{
	if (!layer)
		return FrameStatus::SKIP;
	if (swapchain_dirty)
		return window_width && window_height ? FrameStatus::SWAPCHAIN_OUTDATED : FrameStatus::SKIP;

	frame_pool = NS::AutoreleasePool::alloc()->init();
	// Waits for a drawable, which paces the frames. nullptr after a second without one.
	drawable = layer->nextDrawable();
	if (!drawable)
	{
		EndFramePool();
		return FrameStatus::SKIP;
	}
	// The layer resizes its drawables with the view, which can happen before the resize event arrives.
	MTL::Texture* texture = drawable->texture();
	if (texture->width() != backbuffer.desc.width || texture->height() != backbuffer.desc.height)
	{
		window_width = (uint32)texture->width();
		window_height = (uint32)texture->height();
		swapchain_dirty = true;
		EndFramePool();
		return FrameStatus::SWAPCHAIN_OUTDATED;
	}
	backbuffer.texture = texture;
	command_buffer = queue->commandBuffer();
	command_buffer->addCompletedHandler([](MTL::CommandBuffer* completed) {
		if (completed->status() == MTL::CommandBufferStatusError)
			fprintf(stderr, "Metal command buffer failed: %s\n", completed->error()->localizedDescription()->utf8String());
	});
	return FrameStatus::OK;
}

static RenderPass* CreateRenderPass(const RenderPassDesc& desc)
{
	MTL_ASSERT(desc.attachments.data && desc.attachments.count);
	MetalRenderPass* render_pass = new MetalRenderPass;
	render_pass->attachments.assign(desc.attachments.data, desc.attachments.data + desc.attachments.count);
	return reinterpret_cast<RenderPass*>(render_pass);
}

static void DestroyRenderPass(RenderPass* render_pass)
{
	delete ToImpl(render_pass);
}

static Framebuffer* CreateFramebuffer(FramebufferDesc&& desc)
{
	MTL_ASSERT(desc.render_pass && desc.attachments.count == ToImpl(desc.render_pass)->attachments.size());
	MetalFramebuffer* framebuffer = new MetalFramebuffer;
	framebuffer->attachments.resize(desc.attachments.count);
	for (uint32 i = 0; i < desc.attachments.count; ++i)
	{
		MetalTexture* texture = ToImpl(desc.attachments[i]);
		if (i == 0)
		{
			framebuffer->width = texture->desc.width;
			framebuffer->height = texture->desc.height;
		}
		framebuffer->attachments[i] = texture;
	}
	return reinterpret_cast<Framebuffer*>(framebuffer);
}

static void DestroyFramebuffer(Framebuffer* framebuffer)
{
	delete ToImpl(framebuffer);
}

// The shader's MSL compiled by Metal, or nullptr with the errors printed. Metal's compiler is LLVM based, and fast math
// would let it assume floats are never NaN or infinite, which the other targets don't, so it's off.
static MTL::Function* CompileMSL(const CompiledEntryPoint& shader)
{
	NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
	DEFER(pool->release());
	MTL::CompileOptions* options = MTL::CompileOptions::alloc()->init();
	DEFER(options->release());
	options->setLanguageVersion(MTL::LanguageVersion2_0);
	options->setFastMathEnabled(false);
	NS::Error* error = nullptr;
	NS::String* source = NS::String::string((const char*)shader.code.data, NS::UTF8StringEncoding);
	MTL::Library* library = device->newLibrary(source, options, &error);
	if (!library)
	{
		fprintf(stderr, "Metal failed to compile a shader: %s\n",
			error ? error->localizedDescription()->utf8String() : "no message");
		return nullptr;
	}
	DEFER(library->release());
	MTL::Function* function = library->newFunction(NS::String::string(MSL_ENTRY_POINT_NAME, NS::UTF8StringEncoding));
	MTL_ASSERT(function);
	return function;
}

static Pipeline* CreatePipeline(const CreatePipelineOptions& options)
{
	MTL_ASSERT(options.render_pass);
	MetalRenderPass* render_pass = ToImpl(options.render_pass);
	NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
	DEFER(pool->release());
	MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
	DEFER(descriptor->release());

	std::vector<MTL::Function*> functions;
	DEFER(for (MTL::Function* function : functions) function->release());
	for (uint32 i = 0; i < options.shaders.count; ++i)
	{
		const CompiledEntryPoint& shader = options.shaders[i];
		MTL::Function* function = CompileMSL(shader);
		if (!function)
			return nullptr;
		functions.push_back(function);
		if (shader.stage == ShaderStage::VERTEX)
			descriptor->setVertexFunction(function);
		else
			descriptor->setFragmentFunction(function);
	}
	MTL_ASSERT(descriptor->vertexFunction());

	uint32 color_count = 0;
	for (const AttachmentDesc& attachment : render_pass->attachments)
	{
		MTL::PixelFormat format = ToMTLPixelFormat(attachment.format);
		if (attachment.state_during == ImageState::COLOR_ATTACHMENT)
			descriptor->colorAttachments()->object(color_count++)->setPixelFormat(format);
		else
		{
			descriptor->setDepthAttachmentPixelFormat(format);
			if (attachment.format == TextureFormat::D24_UNORM_S8_UINT)
				descriptor->setStencilAttachmentPixelFormat(format);
		}
	}

	NS::Error* error = nullptr;
	MTL::RenderPipelineState* state = device->newRenderPipelineState(descriptor, &error);
	if (!state)
	{
		fprintf(stderr, "Metal failed to create a pipeline: %s\n",
			error ? error->localizedDescription()->utf8String() : "no message");
		return nullptr;
	}
	MetalPipeline* pipeline = new MetalPipeline;
	pipeline->state = state;
	return reinterpret_cast<Pipeline*>(pipeline);
}

static void DestroyPipeline(Pipeline* pipeline)
{
	if (!pipeline)
		return;
	// Command buffers keep what they use alive, so there's nothing to wait for.
	MetalPipeline* impl = ToImpl(pipeline);
	impl->state->release();
	delete impl;
}

static void CmdBeginRenderPass(const RenderPassBeginDesc& desc)
{
	MTL_ASSERT(desc.render_pass && desc.framebuffer && !encoder);
	MetalRenderPass* render_pass = ToImpl(desc.render_pass);
	MetalFramebuffer* framebuffer = ToImpl(desc.framebuffer);
	MTL::RenderPassDescriptor* descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
	uint32 color_count = 0;
	for (uint32 i = 0; i < render_pass->attachments.size(); ++i)
	{
		const AttachmentDesc& attachment = render_pass->attachments[i];
		MTL::Texture* texture = framebuffer->attachments[i]->texture;
		MTL_ASSERT(texture);
		ClearValue clear = desc.clear_values.data && i < desc.clear_values.count ? desc.clear_values[i] : ClearValue{};
		MTL::LoadAction load = ToMTLLoadAction(attachment.load_op);
		MTL::StoreAction store = ToMTLStoreAction(attachment.store_op);
		if (attachment.state_during == ImageState::COLOR_ATTACHMENT)
		{
			MTL::RenderPassColorAttachmentDescriptor* color = descriptor->colorAttachments()->object(color_count++);
			color->setTexture(texture);
			color->setLoadAction(load);
			color->setStoreAction(store);
			color->setClearColor(MTL::ClearColor(clear.color[0], clear.color[1], clear.color[2], clear.color[3]));
			continue;
		}
		MTL::RenderPassDepthAttachmentDescriptor* depth = descriptor->depthAttachment();
		depth->setTexture(texture);
		depth->setLoadAction(load);
		depth->setStoreAction(store);
		depth->setClearDepth(clear.depth);
		if (attachment.format == TextureFormat::D24_UNORM_S8_UINT)
		{
			MTL::RenderPassStencilAttachmentDescriptor* stencil = descriptor->stencilAttachment();
			stencil->setTexture(texture);
			stencil->setLoadAction(load);
			stencil->setStoreAction(store);
			stencil->setClearStencil(clear.stencil);
		}
	}
	encoder = command_buffer->renderCommandEncoder(descriptor);
	encoder->setViewport(MTL::Viewport{
		.originX = 0.0,
		.originY = 0.0,
		.width = (double)framebuffer->width,
		.height = (double)framebuffer->height,
		.znear = 0.0,
		.zfar = 1.0,
	});
}

static void CmdEndRenderPass()
{
	encoder->endEncoding();
	encoder = nullptr;
}

static void CmdBindPipeline(Pipeline* pipeline)
{
	MTL_ASSERT(pipeline && encoder);
	encoder->setRenderPipelineState(ToImpl(pipeline)->state);
	// Clockwise on screen is the front, like every backend's. Metal's clip space and framebuffer coordinates are D3D's,
	// so unlike Vulkan's, the vertex shader doesn't flip Y.
	encoder->setFrontFacingWinding(MTL::WindingClockwise);
	encoder->setCullMode(MTL::CullModeNone);
}

static void CmdDraw(uint32 vertex_count, uint32 first_vertex)
{
	encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, first_vertex, vertex_count);
}

static void EndFrame()
{
	MTL_ASSERT(!encoder);
	command_buffer->presentDrawable(drawable);
	command_buffer->commit();
	if (last_command_buffer)
		last_command_buffer->release();
	last_command_buffer = command_buffer->retain();
	EndFramePool();
}

static void Shutdown()
{
	if (last_command_buffer)
	{
		last_command_buffer->waitUntilCompleted();
		last_command_buffer->release();
	}
	if (queue)
		queue->release();
	if (device)
		device->release();
	device = nullptr;
	queue = nullptr;
	layer = nullptr;
	backbuffer = {};
	swapchain_dirty = false;
	window_width = 0;
	window_height = 0;
	last_command_buffer = nullptr;
}

static void HandlePALEvent(const PAL::Event& event)
{
	switch (event.type)
	{
	case PAL::EventType::WINDOW_RESIZE:
		window_width = (uint32)event.width;
		window_height = (uint32)event.height;
		swapchain_dirty = window_width != backbuffer.desc.width || window_height != backbuffer.desc.height;
		break;
	default:
		break;
	}
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
	CGSize size = layer->drawableSize();
	window_width = (uint32)size.width;
	window_height = (uint32)size.height;
	backbuffer.desc = {
		.width = window_width,
		.height = window_height,
		.format = TextureFormat::BGRA8_UNORM,
	};
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
		.backbuffer_format = backbuffer.desc.format,
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
