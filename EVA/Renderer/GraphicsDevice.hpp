#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct SDL_Window;

struct VkBuffer_T;
struct VkImage_T;
struct VkImageView_T;
struct VkSampler_T;
struct VkShaderModule_T;
struct VkPipeline_T;
struct VkPipelineLayout_T;
struct VkCommandBuffer_T;
struct VkFence_T;
struct VkDeviceMemory_T;
struct VmaAllocation_T;

using VkBuffer         = VkBuffer_T*;
using VkImage          = VkImage_T*;
using VkImageView      = VkImageView_T*;
using VkSampler        = VkSampler_T*;
using VkShaderModule   = VkShaderModule_T*;
using VkPipeline       = VkPipeline_T*;
using VkPipelineLayout = VkPipelineLayout_T*;
using VkCommandBuffer  = VkCommandBuffer_T*;
using VkFence          = VkFence_T*;
using VkDeviceMemory   = VkDeviceMemory_T*;
using VmaAllocation    = VmaAllocation_T*;

namespace GFX {

struct BindlessIndexAllocator {
	U32 nextIndex = 0;
	U32 capacity = 0;
	std::vector<U32> freeList;

	void Init(U32 newCapacity) {
		nextIndex = 0;
		capacity = newCapacity;
		freeList.clear();
		freeList.reserve(newCapacity);
	}

	U32 Allocate() {
		if (!freeList.empty()) {
			U32 index = freeList.back();
			freeList.pop_back();
			return index;
		}
		if (nextIndex >= capacity) Fatal("Bindless index allocator exhausted (%u entries)", capacity);
		return nextIndex++;
	}

	void Free(U32 index) {
		assert(index < nextIndex);
		freeList.push_back(index);
	}
};

class GPUBuffer;
class Image;
class Sampler;
class ShaderModule;
class GraphicsPipeline;
struct RenderingDesc;
struct BufferCopy;
struct BufferImageCopy;
struct ImageBarrier;

enum class GraphicsAPI : U8 {
	None,
	Vulkan,
};

enum class Format : U16 {
	None,

	R8_UNORM,
	R8_SNORM,
	R8_UINT,
	R8_SINT,
	R8_SRGB,
	RG8_UNORM,
	RG8_SNORM,
	RG8_UINT,
	RG8_SINT,
	RG8_SRGB,
	RGB8_UNORM,
	RGB8_SNORM,
	RGB8_UINT,
	RGB8_SINT,
	RGB8_SRGB,
	BGR8_UNORM,
	BGR8_SNORM,
	BGR8_UINT,
	BGR8_SINT,
	BGR8_SRGB,
	RGBA8_UNORM,
	RGBA8_SNORM,
	RGBA8_UINT,
	RGBA8_SINT,
	RGBA8_SRGB,
	BGRA8_UNORM,
	BGRA8_SNORM,
	BGRA8_UINT,
	BGRA8_SINT,
	BGRA8_SRGB,

	R16_UNORM,
	R16_SNORM,
	R16_UINT,
	R16_SINT,
	R16_FLOAT,
	RG16_UNORM,
	RG16_SNORM,
	RG16_UINT,
	RG16_SINT,
	RG16_FLOAT,
	RGB16_UNORM,
	RGB16_SNORM,
	RGB16_UINT,
	RGB16_SINT,
	RGB16_FLOAT,
	RGBA16_UNORM,
	RGBA16_SNORM,
	RGBA16_UINT,
	RGBA16_SINT,
	RGBA16_FLOAT,

	R32_UINT,
	R32_SINT,
	R32_FLOAT,
	RG32_UINT,
	RG32_SINT,
	RG32_FLOAT,
	RGB32_UINT,
	RGB32_SINT,
	RGB32_FLOAT,
	RGBA32_UINT,
	RGBA32_SINT,
	RGBA32_FLOAT,

	D16_UNORM,
	D32_FLOAT,
	D16_UNORM_S8_UINT,
	D24_UNORM_S8_UINT,
	D32_FLOAT_S8_UINT,
};

enum class MemoryUsage : U8 {
	GPUOnly,
	CPUToGPU,
	GPUToCPU,
};

enum GPUBufferUsageBits : U32 {
	GPUBufferUsage_None           = 0,
	GPUBufferUsage_TransferSource = 1 << 0,
	GPUBufferUsage_TransferDest   = 1 << 1,
	GPUBufferUsage_VertexBuffer   = 1 << 2,
	GPUBufferUsage_IndexBuffer    = 1 << 3,
	GPUBufferUsage_ConstantBuffer = 1 << 4,
	GPUBufferUsage_StorageBuffer  = 1 << 5,
	GPUBufferUsage_IndirectBuffer = 1 << 6,
};
using GPUBufferUsage = U32;

enum ImageUsageBits : U32 {
	ImageUsage_None            = 0,
	ImageUsage_TransferSource  = 1 << 0,
	ImageUsage_TransferDest    = 1 << 1,
	ImageUsage_Sampled         = 1 << 2,
	ImageUsage_Storage         = 1 << 3,
	ImageUsage_ColorAttachment = 1 << 4,
	ImageUsage_DepthAttachment = 1 << 5,
};
using ImageUsage = U32;

enum class ImageState : U8 {
	Undefined,
	General,
	TransferSource,
	TransferDest,
	ColorAttachment,
	DepthAttachment,
	DepthReadOnly,
	ShaderRead,
	Present,
};

enum class PrimitiveTopology : U8 {
	PointList,
	LineList,
	LineStrip,
	TriangleList,
	TriangleStrip,
	TriangleFan,
	LineListWithAdjacency,
	LineStripWithAdjacency,
	TriangleListWithAdjacency,
	TriangleStripWithAdjacency,
	PatchList,
};

enum class PolygonMode : U8 {
	Fill,
	Line,
	Point,
};

enum class CullMode : U8 {
	None,
	Front,
	Back,
	FrontAndBack,
};

enum class FrontFace : U8 {
	Clockwise,
	CounterClockwise,
};

enum class CompareOp : U8 {
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always,
};

enum class BlendFactor : U8 {
	Zero,
	One,
	SourceColor,
	OneMinusSourceColor,
	DestColor,
	OneMinusDestColor,
	SourceAlpha,
	OneMinusSourceAlpha,
	DestAlpha,
	OneMinusDestAlpha,
	ConstantColor,
	OneMinusConstantColor,
	ConstantAlpha,
	OneMinusConstantAlpha,
	SourceAlphaSaturate,
	Source1Color,
	OneMinusSource1Color,
	Source1Alpha,
	OneMinusSource1Alpha,
};

enum class BlendOp : U8 {
	Add,
	Subtract,
	ReverseSubtract,
	Min,
	Max,
};

enum class Filter : U8 {
	Nearest,
	Linear,
};

enum class MipmapMode : U8 {
	Nearest,
	Linear,
};

enum class AddressMode : U8 {
	Repeat,
	MirroredRepeat,
	ClampToEdge,
	MirrorClampToEdge,
};

enum class IndexType : U8 {
	U16,
	U32,
};

enum class LoadOp : U8 {
	Load,
	Clear,
	DontCare,
};

enum class StoreOp : U8 {
	Store,
	DontCare,
};

class GPUBuffer : public Object {
public:
	ECLASS_COMMON();

	U64   m_size          = 0;
	U32   m_bindlessIndex = UINT32_MAX;
	void* m_mapped        = nullptr;

	union {
		VkBuffer m_vk = nullptr;
	};

	union {
		VkDeviceMemory m_vkMemory = nullptr;
	};
	VmaAllocation m_vmaAllocation = nullptr;
};

class Image : public Object {
public:
	ECLASS_COMMON();
	Image() : m_vulkan{} {}

	U32        m_width            = 0;
	U32        m_height           = 0;
	U32        m_mipCount         = 1;
	U32        m_bindlessIndex    = UINT32_MAX;
	Format     m_format           = Format::None;
	ImageState m_state            = ImageState::Undefined;
	bool       m_ownedBySwapchain = false;

	union {
		struct {
			VkImage        image      = nullptr;
			VkImageView    imageView  = nullptr;
			VkDeviceMemory memory     = nullptr;
			VmaAllocation  allocation = nullptr;
		} m_vulkan;
	};
};

class Sampler : public Object {
public:
	ECLASS_COMMON();

	U32 m_bindlessIndex = UINT32_MAX;

	union {
		VkSampler m_vk = nullptr;
	};
};

class ShaderModule : public Object {
public:
	ECLASS_COMMON();
	union {
		VkShaderModule m_vk = nullptr;
	};
};

class GraphicsPipeline : public Object {
public:
	ECLASS_COMMON();
	GraphicsPipeline() : m_vulkan{} {}

	union {
		struct {
			VkPipeline pipeline = nullptr;
			VkPipelineLayout layout = nullptr;
		} m_vulkan;
	};
};

class CommandBuffer : public Object {
public:
	ECLASS_COMMON();

	union {
		VkCommandBuffer m_vk = nullptr;
	};

	virtual void BeginRendering(const RenderingDesc& desc) = 0;
	virtual void EndRendering(const RenderingDesc& desc) = 0;

	virtual void BindPipeline(GraphicsPipeline* pipeline) = 0;
	virtual void BindIndexBuffer(GPUBuffer* buffer, IndexType type, U64 offset = 0) = 0;
	virtual void PushConstants(GraphicsPipeline* pipeline, U32 size, const void* data) = 0;

	virtual void Draw(U32 vertexCount, U32 instanceCount = 1, U32 firstVertex = 0, U32 firstInstance = 0) = 0;
	virtual void DrawIndexed(U32 indexCount, U32 instanceCount = 1, U32 firstIndex = 0, I32 vertexOffset = 0, U32 firstInstance = 0) = 0;

	virtual void CopyBuffer(GPUBuffer* source, GPUBuffer* destination, const BufferCopy& copy) = 0;
	virtual void CopyBufferToImage(GPUBuffer* source, Image* destination, const BufferImageCopy& copy) = 0;
	// TODO: Make this API accept multiple basrriers
	virtual void ImageBarrier(const GFX::ImageBarrier& barrier) = 0;
	virtual void GenerateMipmaps(Image* image) = 0;
};

class Fence : public Object {
public:
	ECLASS_COMMON();

	union {
		VkFence m_vk = nullptr;
	};
};

struct GPUBufferDesc {
	U64            size        = 0;
	GPUBufferUsage usage       = GPUBufferUsage_None;
	MemoryUsage    memoryUsage = MemoryUsage::GPUOnly;
	bool           bindless     = false;
};

struct ImageDesc {
	U32         width         = 0;
	U32         height        = 0;
	U32         mipCount      = 1;
	Format      format        = Format::None;
	ImageUsage  usage         = ImageUsage_None;
	ImageState  initialState  = ImageState::Undefined;
	bool        bindless      = false;
	bool        ownedBySwapchain = false;
	VkImage     existingImage = nullptr;
};

struct SamplerDesc {
	Filter      minFilter         = Filter::Linear;
	Filter      magFilter         = Filter::Linear;
	MipmapMode  mipmapMode        = MipmapMode::Linear;
	AddressMode addressU          = AddressMode::Repeat;
	AddressMode addressV          = AddressMode::Repeat;
	AddressMode addressW          = AddressMode::Repeat;
	float       mipLodBias        = 0.0f;
	float       minLod            = 0.0f;
	float       maxLod            = 1000.0f;
	float       maxAnisotropy     = 1.0f;
	bool        anisotropyEnable  = false;
	bool        bindless          = false;
};

struct ShaderModuleDesc {
	const U32*  code      = nullptr;
	U64         codeSize  = 0;
};

struct RenderPassFormat {
	Format colorFormat[4] = {};
	Format depthFormat = Format::None;
};

struct GraphicsPipelineDesc {
	ShaderModule* vertexShader   = nullptr;
	ShaderModule* fragmentShader = nullptr;

	PrimitiveTopology topology     = PrimitiveTopology::TriangleList;
	PolygonMode       polygonMode = PolygonMode::Fill;
	CullMode          cullMode    = CullMode::Back;
	FrontFace         frontFace   = FrontFace::CounterClockwise;

	bool      depthTestEnable  = false;
	bool      depthWriteEnable = false;
	CompareOp depthCompare     = CompareOp::LessEqual;

	bool        blendEnable      = false;
	BlendFactor sourceColorBlend = BlendFactor::SourceAlpha;
	BlendFactor destColorBlend   = BlendFactor::OneMinusSourceAlpha;
	BlendOp     colorBlendOp     = BlendOp::Add;
	BlendFactor sourceAlphaBlend = BlendFactor::One;
	BlendFactor destAlphaBlend   = BlendFactor::OneMinusSourceAlpha;
	BlendOp     alphaBlendOp     = BlendOp::Add;

	RenderPassFormat format = {};

	U32 pushConstantSize = 0;
};

struct AttachmentDesc {
	Image* image = nullptr;

	LoadOp  loadOp  = LoadOp::Load;
	StoreOp storeOp = StoreOp::Store;

	float4 clearColor   = {};
	float  clearDepth   = 1.0f;
	U32    clearStencil = 0;

	ImageState stateBefore = ImageState::Undefined;
	ImageState stateDuring = ImageState::ColorAttachment;
	ImageState stateAfter  = ImageState::Undefined;
};

struct RenderingDesc {
	U32                   colorAttachmentCount = 0;
	const AttachmentDesc* colorAttachments      = nullptr;
	const AttachmentDesc* depthAttachment       = nullptr;
};

struct BufferCopy {
	U64 sourceOffset = 0;
	U64 destOffset   = 0;
	U64 size          = 0;
};

struct BufferImageCopy {
	U64 bufferOffset = 0;
	U32 imageMip     = 0;
	U32 imageLayer   = 0;
	U32 imageX       = 0;
	U32 imageY       = 0;
	U32 imageZ       = 0;
	U32 width         = 0;
	U32 height        = 0;
	U32 depth         = 1;
};

struct ImageBarrier {
	Image* image = nullptr;

	ImageState stateBefore = ImageState::Undefined;
	ImageState stateAfter  = ImageState::Undefined;

	U32 baseMip    = 0;
	U32 mipCount   = 1;
	U32 baseLayer  = 0;
	U32 layerCount = 1;
};

struct SubmitDesc {
	U32                  commandBufferCount = 0;
	CommandBuffer* const* commandBuffers     = nullptr;
	Fence*                fence                = nullptr;
};

struct GraphicsDeviceInitDesc {
	GraphicsAPI api          = GraphicsAPI::Vulkan;
	SDL_Window* window       = nullptr;
	U32         frameCount   = 2;
	bool        enableDebug  = true;
	bool        vsync        = true;
	ImageUsage  swapchainImageUsage = ImageUsage_ColorAttachment;
};

class GraphicsDevice : public Object {
public:
	ECLASS_COMMON();
	static constexpr size_t FrameUploadBufferSize = 16 * 1024 * 1024;

	static Result Create(const GraphicsDeviceInitDesc& desc);
	virtual Result Init(const GraphicsDeviceInitDesc& desc) = 0;
	static void Shutdown();
	static GraphicsDevice* Get();

	bool BeginFrame();
	virtual void EndFrame() = 0;

	virtual CommandBuffer* GetMainCommandBuffer() = 0;
	virtual CommandBuffer* GetTransferCommandBuffer() = 0;

	virtual Image* GetCurrentBackbuffer() = 0;

	virtual void SetVSync(bool enabled) = 0;
	virtual void WaitIdle() = 0;

	virtual CommandBuffer* CreateCommandBuffer() = 0;
	virtual void DestroyCommandBuffer(CommandBuffer* cmd) = 0;
	virtual void BeginCommandBuffer(CommandBuffer* cmd) = 0;
	virtual void EndCommandBuffer(CommandBuffer* cmd) = 0;

	CommandBuffer* BeginImmediateCommandBuffer();
	void FlushImmediateCommandBuffer(CommandBuffer* cmd);

	virtual Fence* CreateFence(bool signaled = false) = 0;
	virtual void DestroyFence(Fence* fence) = 0;
	virtual void WaitForFence(Fence* fence) = 0;
	virtual void ResetFence(Fence* fence) = 0;

	virtual void Submit(const SubmitDesc& desc) = 0;

	virtual GPUBuffer* CreateGPUBuffer(const GPUBufferDesc& desc) = 0;
	virtual void DestroyGPUBuffer(GPUBuffer* buffer) = 0;

	virtual Image* CreateImage(const ImageDesc& desc) = 0;
	virtual void DestroyImage(Image* image) = 0;

	void UploadFrameData(const void* data, size_t size, GPUBuffer** outBuffer, size_t* outBufferOffset);
	void UploadBuffer(GPUBuffer* dest, size_t size, size_t offset, const void* data);
	void UploadImage(Image* dest, size_t size, const void* data);

	virtual Sampler* CreateSampler(const SamplerDesc& desc) = 0;
	virtual void DestroySampler(Sampler* sampler) = 0;

	virtual ShaderModule* CreateShaderModule(const ShaderModuleDesc& desc) = 0;
	virtual void DestroyShaderModule(ShaderModule* shader) = 0;

	virtual GraphicsPipeline* CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
	virtual void DestroyGraphicsPipeline(GraphicsPipeline* pipeline) = 0;

protected:
	virtual bool BeginFrameImpl() = 0;

	GPUBuffer* m_frameUploadBuffer = nullptr;
	size_t     m_frameUploadOffset = 0;
};

}
