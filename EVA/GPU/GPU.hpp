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
struct Buffer;

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
	Slice<ReflectedBindGroup> bind_groups; // the shaders' module's, from the same compile
};

enum BufferUsage : uint32
{
	BUFFER_VERTEX = 1,
	BUFFER_INDEX = 2,
	BUFFER_UNIFORM = 4, // alone: D3D11 can't bind a constant buffer as anything else
};

struct BufferDesc
{
	uint64 size = 0;
	uint32 usage = 0; // BufferUsage bits
};

// What every backend's bind group starts with, so cursors work the same on all of them. Created from a group's
// reflection, which has to outlive it, and filled through cursors (Docs/Plan/Bindings.md, 8.3).
struct BindGroup
{
	uint32 index = 0;
	const TypeLayout* type = nullptr; // the group's struct
	uint64 layout_hash = 0;
	Slice<uint8> constants;           // the implicit uniform buffer's contents, uploaded when the group is used
	bool constants_changed = false;
};

// A position in a bind group and the type there. An invalid cursor, from an unknown field or an index out of range,
// has no type, and so do the cursors made from it; writing through one fails.
struct ShaderCursor
{
	BindGroup* group = nullptr;
	const TypeLayout* type = nullptr;
	uint32 bytes = 0; // into the group's constants

	bool IsValid() const { return type != nullptr; }
	ShaderCursor Field(StringView name) const;
	ShaderCursor Element(uint32 index) const;
	// Plain data laid out as the type is, exactly its size.
	bool Write(const void* data, uint32 size) const;
};

ShaderCursor GetCursor(BindGroup* group);

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
	Buffer* (*CreateBuffer)(const BufferDesc&) = nullptr;
	void (*DestroyBuffer)(Buffer*) = nullptr;
	// A BUFFER_UNIFORM buffer is uploaded whole on D3D11. Outside render passes.
	void (*CmdUploadBuffer)(Buffer*, uint64 offset, Slice<uint8> data) = nullptr;
	// Uploads to the buffer finish before it's read with usage (BufferUsage bits).
	void (*CmdBufferBarrier)(Buffer*, uint32 usage) = nullptr;
	BindGroup* (*CreateBindGroup)(const ReflectedBindGroup&) = nullptr;
	void (*DestroyBindGroup)(BindGroup*) = nullptr;
	// group's index is the one it's for. Every group the pipeline's shaders use has to be set, with an equal layout,
	// when drawing.
	void (*CmdSetBindGroup)(BindGroup* group) = nullptr;
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
