#pragma once
#include <EVA/Core/Basic.hpp>

namespace GFX {

enum class GraphicsAPI {
	None = 0,
	OpenGL,
	Vulkan,
};

// roughly corresponds to dx12 image states, trivially emulated in vulkan. ogl ignores these.
enum class ImageState {
	Undefined    = 0,
	RenderTarget = 1,
	DepthTarget  = 2,
	SampledImage = 3,
};

enum class Heap {
	None         = 0,
	SystemMemory = 1,
	VRAM         = 2,
};

enum GPUBufferUsageBits {
	GPUBufferUsage_None           = 0x00,
	GPUBufferUsage_VertexBuffer   = 0x01,
	GPUBufferUsage_IndexBuffer    = 0x02,
	GPUBufferUsage_StorageBuffer  = 0x04,
	GPUBufferUsage_ConstantBuffer = 0x08,
};
typedef U32 GPUBufferUsage;

enum class Format {
	None = 0,
};

struct ImageDesc {
	Format format = Format::None;
	Heap   heap   = Heap::VRAM;
	U32    width  = 0;
	U32    height = 0;
};

struct GPUBufferDesc {
	GPUBufferUsage usage = GPUBufferUsage_None;
	Heap           heap  = Heap::VRAM;
	U32            size  = 0;
};

class CommandBuffer : Object {
public:
	ECLASS_COMMON();
};

class Image : Object {
public:
	union {
		U32 opengl_handle = 0;
	};
	ECLASS_COMMON();
};

class GPUBuffer : Object {
public:
	union {
		U32 opengl_handle = 0;
	};
	ECLASS_COMMON();
};

class GraphicsDevice : Object {
public:
	ECLASS_COMMON();

	virtual CommandBuffer* BeginCommandBuffer() = 0;
	virtual void EndCommandBuffer(CommandBuffer* cmd);

	// returns true if a frame should be rendered, false if it shouldn't (e.g. game is minimized)
	virtual bool BeginFrame();
	void EndFrame();

	Image*     CreateImage(const ImageDesc& desc);
	void       DestroyImage(Image* image);
	GPUBuffer* CreateGPUBuffer(const GPUBufferDesc& desc);
	void       DestroyBuffer(GPUBuffer* buffer);

	static void Init(GraphicsAPI* preferred_api);
	static GraphicsDevice* Get();
};

};