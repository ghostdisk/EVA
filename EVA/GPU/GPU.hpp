#pragma once
#include <EVA/Core/Common.hpp>

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
};

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

struct PipelineShaderOptions
{
	ShaderStage shader_stage = ShaderStage::VERTEX;
};

struct PipelineCreateOptions
{
	Slice<PipelineShaderOptions> shaders = {};
	RenderPass* render_pass = nullptr;
};

struct Device
{
	TextureFormat backbuffer_format = TextureFormat::RGBA8_UNORM;
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
	Pipeline* (*CreatePipeline)(const PipelineCreateOptions& create_options) = nullptr;
	void (*destroy)(Pipeline* Pipeline) = nullptr;
	void (*BeginRenderPass)(const RenderPassBeginDesc&) = nullptr;
	void (*EndRenderPass)() = nullptr;
	void (*EndFrame)() = nullptr;
};

enum class Backend
{
	NONE = 0,
	D3D11,
	VULKAN,
	METAL,
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
