#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Core/GPUShared.hpp>

namespace EVA::PAL
{
struct Window;
struct Event;
}

namespace EVA::GPU
{

struct Texture;
struct RenderPass;
struct Framebuffer;
struct Pipeline;

enum class TextureFormat
{
	RGBA8_UNORM,
	BGRA8_UNORM,
	D24_UNORM_S8_UINT,
	D32_FLOAT_S8_UINT,
};

inline bool IsDepthStencilFormat(TextureFormat format)
{
	return format == TextureFormat::D24_UNORM_S8_UINT || format == TextureFormat::D32_FLOAT_S8_UINT;
}

struct TextureDesc
{
	uint32 width = 0;
	uint32 height = 0;
	uint32 layers = 1;
	uint32 mip_levels = 1;
	TextureFormat format = TextureFormat::RGBA8_UNORM;
};

enum class ImageState
{
	UNDEFINED,
	COLOR_ATTACHMENT,
	DEPTH_STENCIL_ATTACHMENT,
	SHADER_READ,
	PRESENT,
};

enum class AttachmentLoadOp
{
	LOAD,
	CLEAR,
	DONT_CARE,
};

enum class AttachmentStoreOp
{
	STORE,
	DONT_CARE,
};

struct AttachmentDesc
{
	TextureFormat format = TextureFormat::RGBA8_UNORM;
	AttachmentLoadOp load_op = AttachmentLoadOp::LOAD;
	AttachmentStoreOp store_op = AttachmentStoreOp::STORE;
	ImageState state_before = ImageState::UNDEFINED;
	ImageState state_during = ImageState::COLOR_ATTACHMENT;
	ImageState state_after = ImageState::COLOR_ATTACHMENT;
};

struct RenderPassDesc
{
	Slice<AttachmentDesc> attachments;
};

struct FramebufferDesc
{
	RenderPass* render_pass = nullptr;
	Slice<Texture*> attachments;
};

struct ClearValue
{
	float color[4] = {};
	float depth = 1.0f;
	uint8 stencil = 0;
};

struct RenderPassBeginDesc
{
	RenderPass* render_pass = nullptr;
	Framebuffer* framebuffer = nullptr;
	Slice<ClearValue> clear_values;
};

enum class FrameStatus
{
	OK,
	SKIP,
	// Destroy everything that references backbuffers, then call RecreateSwapchain.
	SWAPCHAIN_OUTDATED,
};

// A graphics pipeline, kept minimal until there's more to draw than a triangle: triangle lists without vertex buffers,
// no culling, no depth, no blending, writing every channel of each of the render pass's color attachments.
struct CreatePipelineOptions
{
	Slice<CompiledEntryPoint> shaders;  // a VERTEX one and optionally a FRAGMENT one, compiled for the device's backend
	RenderPass* render_pass = nullptr;  // its subpass, whose color attachments get the fragment shader's outputs
};

// Device functions starting with Cmd record commands into the current frame, between BeginFrame and EndFrame.
struct Device
{
	Backend backend = Backend::NONE;
	TextureFormat backbuffer_format = TextureFormat::RGBA8_UNORM;
	// A depth-stencil format the device can render to: D24_UNORM_S8_UINT, or D32_FLOAT_S8_UINT where there's no 24-bit
	// depth, like on Apple GPUs and some of AMD's on Vulkan.
	TextureFormat depth_format = TextureFormat::D24_UNORM_S8_UINT;
	void (*Shutdown)() = nullptr;
	void (*HandlePALEvent)(const PAL::Event&) = nullptr;
	RenderPass* (*CreateRenderPass)(const RenderPassDesc&) = nullptr;
	void (*DestroyRenderPass)(RenderPass*) = nullptr;
	Framebuffer* (*CreateFramebuffer)(FramebufferDesc&&) = nullptr;
	void (*DestroyFramebuffer)(Framebuffer*) = nullptr;
	uint32 (*GetBackbufferCount)() = nullptr;
	Texture* (*GetBackbuffer)(uint32 index) = nullptr;
	TextureDesc (*GetTextureDesc)(Texture*) = nullptr;
	bool (*RecreateSwapchain)() = nullptr;
	FrameStatus (*BeginFrame)() = nullptr;
	uint32 (*GetCurrentBackbufferIndex)() = nullptr;
	// nullptr with the reason printed if a shader doesn't compile for the device, like when fxc runs out of registers.
	Pipeline* (*CreatePipeline)(const CreatePipelineOptions&) = nullptr;
	void (*DestroyPipeline)(Pipeline*) = nullptr;
	void (*CmdBeginRenderPass)(const RenderPassBeginDesc&) = nullptr; // the viewport and scissor cover the framebuffer
	void (*CmdEndRenderPass)() = nullptr;
	void (*CmdBindPipeline)(Pipeline*) = nullptr; // in a render pass compatible with the pipeline's
	void (*CmdDraw)(uint32 vertex_count, uint32 first_vertex) = nullptr;
	void (*EndFrame)() = nullptr;
};

struct InitOptions
{
	PAL::Window* window = nullptr;
	Backend preferred_backend = Backend::NONE;
	bool debug = false;
};

struct BackendDesc
{
	Backend backend = Backend::NONE;
	bool (*Init)(Device& out_device, const InitOptions& options) = nullptr;
};

void Init(const InitOptions& init_options);
void Shutdown();

extern Device device;

inline void Shutdown()
{
	if (device.Shutdown)
	{
		device.Shutdown();
		device = {};
	}
}

}
