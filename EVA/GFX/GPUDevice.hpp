#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Math.hpp>
#include <EVA/Async/JobSystem.hpp>

struct SDL_Window;
class Promise;

#define GPU_MAX_BUFFERS      4096
#define GPU_MAX_IMAGES       4096
#define GPU_MAX_SAMPLERS     128
#define GPU_MAX_PIPELINES    128

typedef struct VkBuffer_T*         VkBuffer;
typedef struct VkImage_T*          VkImage;
typedef struct VkImageView_T*      VkImageView;
typedef struct VkSampler_T*        VkSampler;
typedef struct VkShaderModule_T*   VkShaderModule;
typedef struct VkPipeline_T*       VkPipeline;
typedef struct VkPipelineLayout_T* VkPipelineLayout;
typedef struct VkCommandBuffer_T*  VkCommandBuffer;
typedef struct VkFence_T*          VkFence;
typedef struct VkDeviceMemory_T*   VkDeviceMemory;
typedef struct VmaAllocation_T*    VmaAllocation;

template <typename T>
class GPUResourcePool {
	U32            m_nextIndex  = 0;
	U32            m_capacity   = 0;
	Vector<U32>    m_freeList   = {};

public:
	Vector<T>      m_values     = {};

	void Init(U32 newCapacity) {
		m_nextIndex = 1;
		m_capacity = newCapacity;
		m_freeList.clear();
		m_values.resize(newCapacity);
	}

	U32 Register(T res, U32 fixedIndex = 0xFFFFFFFF) {
		U32 index = 0;
		if (fixedIndex != 0xFFFFFFFF) {
			index = fixedIndex;
		} else if (!m_freeList.empty()) {
			index = m_freeList.back();
			m_freeList.pop_back();
		} else {
			if (m_nextIndex >= m_capacity)
				Fatal("Bindless index allocator exhausted (%u entries)", m_capacity);
			index = m_nextIndex;
		}

		if (m_nextIndex <= index)
			m_nextIndex = index + 1;

		m_values[index] = res;
		return index;
	}

	void Free(U32 index) {
		assert(index < m_nextIndex);
		m_values[index] = nullptr;
		m_freeList.push_back(index);
	}

	T Get(size_t index) {
		return m_values[index];
	}

};

class GPUBuffer;
class GPUImage;
class GPUSampler;
class GPUShaderModule;
class GPUGraphicsPipeline;
struct GPURenderingDesc;
struct GPUBufferCopyDesc;
struct GPUBufferImageCopyDesc;
struct GPUImageBarrierDesc;

enum class GPUBackend : U8 {
	None,
	Vulkan,
};

enum class GPUFormat : U16 {
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

	DEPTH_FORMAT_START,
	D16_UNORM,
	D32_FLOAT,
	D16_UNORM_S8_UINT,
	D24_UNORM_S8_UINT,
	D32_FLOAT_S8_UINT,
	DEPTH_FORMAT_END,
};

bool GPUFormatIsDepth(GPUFormat format);
bool GPUFormatHasStencil(GPUFormat format);

enum class GPUMemoryUsage : U8 {
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

enum GPUImageUsageBits : U32 {
	GPUImageUsage_None            = 0,
	GPUImageUsage_TransferSource  = 1 << 0,
	GPUImageUsage_TransferDest    = 1 << 1,
	GPUImageUsage_Sampled         = 1 << 2,
	GPUImageUsage_Storage         = 1 << 3,
	GPUImageUsage_ColorAttachment = 1 << 4,
	GPUImageUsage_DepthAttachment = 1 << 5,
};
using GPUImageUsage = U32;

enum class GPUImageState : U8 {
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

enum class GPUPrimitiveTopology : U8 {
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

enum class GPUPolygonMode : U8 {
	Fill,
	Line,
	Point,
};

enum class GPUCullMode : U8 {
	None         = 0,
	Front        = 1,
	Back         = 2,
	FrontAndBack = 3,
};
EAUTO_ENUM(GPUCullMode);

enum class GPUFrontFace : U8 {
	Clockwise,
	CounterClockwise,
};

enum class GPUCompareOp : U8 {
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always,
};

enum class GPUBlendFactor : U8 {
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

enum class GPUBlendOp : U8 {
	Add,
	Subtract,
	ReverseSubtract,
	Min,
	Max,
};

enum class GPUBlendMode : U8 {
	None,
	Solid,
	AlphaBlend,
	Add,
	Multiply,
};
EAUTO_ENUM(GPUBlendMode);

struct GPUBlendState {
	bool        blendEnable      = false;
	GPUBlendFactor sourceColorBlend = GPUBlendFactor::SourceAlpha;
	GPUBlendFactor destColorBlend   = GPUBlendFactor::OneMinusSourceAlpha;
	GPUBlendOp     colorBlendOp     = GPUBlendOp::Add;
	GPUBlendFactor sourceAlphaBlend = GPUBlendFactor::One;
	GPUBlendFactor destAlphaBlend   = GPUBlendFactor::OneMinusSourceAlpha;
	GPUBlendOp     alphaBlendOp     = GPUBlendOp::Add;
};

GPUBlendState GPUBlendModeToBlendState(GPUBlendMode mode);

enum class GPUFilter : U8 {
	Nearest,
	Linear,
};

enum class GPUMipmapMode : U8 {
	Nearest,
	Linear,
};

enum class GPUAddressMode : U8 {
	Repeat,
	MirroredRepeat,
	ClampToEdge,
	MirrorClampToEdge,
};

enum class GPUIndexType : U8 {
	U16,
	U32,
};

enum class GPULoadOp : U8 {
	Load,
	Clear,
	DontCare,
};

enum class GPUStoreOp : U8 {
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

class GPUImage : public Object {
public:
	ECLASS_COMMON();
	GPUImage() : m_vulkan{} {}

	U32        m_width            = 0;
	U32        m_height           = 0;
	U32        m_mipCount         = 1;
	U32        m_bindlessIndex    = UINT32_MAX;
	GPUFormat     m_format           = GPUFormat::None;
	GPUImageState m_state            = GPUImageState::Undefined;
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

class GPUSampler : public Object {
public:
	ECLASS_COMMON();

	U32 m_bindlessIndex = UINT32_MAX;

	union {
		VkSampler m_vk = nullptr;
	};
};

class GPUShaderModule : public Object {
public:
	ECLASS_COMMON();
	union {
		VkShaderModule m_vk = nullptr;
	};
};

class GPUGraphicsPipeline : public Object {
public:
	U32 m_index = 0;

	ECLASS_COMMON();

	union {
		struct {
			VkPipeline       pipeline = {};
			VkPipelineLayout layout   = {};
		} m_vulkan;
	};

	GPUGraphicsPipeline() : m_vulkan{} {}
};

class GPUCommandBuffer : public Object {
public:
	ECLASS_COMMON();

	union {
		VkCommandBuffer m_vk = nullptr;
	};

	virtual void BeginRendering(const GPURenderingDesc& desc) = 0;
	virtual void EndRendering(const GPURenderingDesc& desc) = 0;

	virtual void BindPipeline(GPUGraphicsPipeline* pipeline) = 0;
	virtual void BindIndexBuffer(GPUBuffer* buffer, GPUIndexType type, U64 offset = 0) = 0;
	virtual void PushConstants(GPUGraphicsPipeline* pipeline, U32 size, const void* data) = 0;

	virtual void Draw(U32 vertexCount, U32 instanceCount = 1, U32 firstVertex = 0, U32 firstInstance = 0) = 0;
	virtual void DrawIndexed(U32 indexCount, U32 instanceCount = 1, U32 firstIndex = 0, I32 vertexOffset = 0, U32 firstInstance = 0) = 0;

	virtual void CopyBuffer(GPUBuffer* source, GPUBuffer* destination, const GPUBufferCopyDesc& copy) = 0;
	virtual void CopyBufferToImage(GPUBuffer* source, GPUImage* destination, const GPUBufferImageCopyDesc& copy) = 0;
	// TODO: Make this API accept multiple basrriers
	virtual void ImageBarrier(const GPUImageBarrierDesc& barrier) = 0;
	virtual void GenerateMipmaps(GPUImage* image) = 0;
};

struct GPUBufferDesc {
	String         name        = {};
	U64            size        = 0;
	GPUBufferUsage usage       = GPUBufferUsage_None;
	GPUMemoryUsage    memoryUsage = GPUMemoryUsage::GPUOnly;
	bool           bindless     = false;
};

struct GPUImageDesc {
	String         name        = {};
	U32         width            = 0;
	U32         height           = 0;
	U32         mipCount         = 1;
	GPUFormat      format           = GPUFormat::None;
	GPUImageUsage  usage            = GPUImageUsage_None;
	GPUImageState  initialState     = GPUImageState::Undefined;
	bool        bindless         = false;
	bool        ownedBySwapchain = false;
	VkImage     existingImage    = nullptr;
};

struct GPUSamplerDesc {
	GPUFilter      minFilter         = GPUFilter::Linear;
	GPUFilter      magFilter         = GPUFilter::Linear;
	GPUMipmapMode  mipmapMode        = GPUMipmapMode::Linear;
	GPUAddressMode addressU          = GPUAddressMode::Repeat;
	GPUAddressMode addressV          = GPUAddressMode::Repeat;
	GPUAddressMode addressW          = GPUAddressMode::Repeat;
	float       mipLodBias        = 0.0f;
	float       minLod            = 0.0f;
	float       maxLod            = 1000.0f;
	float       maxAnisotropy     = 1.0f;
	bool        anisotropyEnable  = false;
	bool        compareEnable     = false;
	GPUCompareOp   compareOp         = GPUCompareOp::Always;
	U32         forcedBindlessIndex = 0xFFFFFFFF;
};

struct GPUShaderModuleDesc {
	const U32*  code      = nullptr;
	U64         codeSize  = 0;
};

struct GPURenderPassFormat {
	GPUFormat colorFormat[4] = {};
	GPUFormat depthFormat = GPUFormat::None;
};

struct GPUGraphicsPipelineDesc {
	GPUShaderModule* vertexShader   = nullptr;
	GPUShaderModule* fragmentShader = nullptr;

	GPUPrimitiveTopology topology     = GPUPrimitiveTopology::TriangleList;
	GPUPolygonMode       polygonMode = GPUPolygonMode::Fill;
	GPUCullMode          cullMode    = GPUCullMode::Back;
	GPUFrontFace         frontFace   = GPUFrontFace::CounterClockwise;

	bool      depthTestEnable  = false;
	bool      depthWriteEnable = false;
	GPUCompareOp depthCompare     = GPUCompareOp::LessEqual;

	GPUBlendMode blendMode = GPUBlendMode::Solid;

	GPURenderPassFormat format = {};

	U32 pushConstantSize = 0;
};

struct GPUAttachmentDesc {
	GPUImage* image = nullptr;

	GPULoadOp  loadOp  = GPULoadOp::Load;
	GPUStoreOp storeOp = GPUStoreOp::Store;

	float4 clearColor   = {};
	float  clearDepth   = 1.0f;
	U32    clearStencil = 0;

	GPUImageState stateBefore = GPUImageState::Undefined;
	GPUImageState stateDuring = GPUImageState::ColorAttachment;
	GPUImageState stateAfter  = GPUImageState::Undefined;
};

struct GPURenderingDesc {
	U32                   colorAttachmentCount = 0;
	const GPUAttachmentDesc* colorAttachments      = nullptr;
	const GPUAttachmentDesc* depthAttachment       = nullptr;
};

struct GPUBufferCopyDesc {
	U64 sourceOffset = 0;
	U64 destOffset   = 0;
	U64 size          = 0;
};

struct GPUBufferImageCopyDesc {
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

struct GPUImageBarrierDesc {
	GPUImage* image = nullptr;

	GPUImageState stateBefore = GPUImageState::Undefined;
	GPUImageState stateAfter  = GPUImageState::Undefined;

	U32 baseMip    = 0;
	U32 mipCount   = 1;
	U32 baseLayer  = 0;
	U32 layerCount = 1;
};

struct GPUSwapchainDesc {
	U32         frameCount   = 2;
	bool        vsync        = true;
	GPUImageUsage  imageUsage   = GPUImageUsage_ColorAttachment;
};

struct GPUDeviceInitDesc {
	GPUBackend       backend       = GPUBackend::Vulkan;
	SDL_Window*      window        = nullptr;
	bool             enableDebug   = true;
	GPUSwapchainDesc swapchainDesc = {};
	Promise          signalPromise = {};
};

class GPUDevice : public Object {
public:
	ECLASS_COMMON();
	static constexpr size_t FrameUploadBufferSize = 16 * 1024 * 1024;

	static Result Create(const GPUDeviceInitDesc& desc);
	virtual Result Init(const GPUDeviceInitDesc& desc) = 0;
	static void Shutdown();
	static GPUDevice* Get();

	virtual bool BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual GPUCommandBuffer* GetMainCommandBuffer() = 0;
	virtual GPUCommandBuffer* GetTransferCommandBuffer() = 0;

	virtual GPUImage* GetCurrentBackbuffer() = 0;

	virtual void WaitIdle() = 0;

	virtual GPUCommandBuffer* BeginImmediateCommandBuffer() = 0;
	virtual void FlushImmediateCommandBuffer(GPUCommandBuffer* cmd) = 0;

	virtual GPUBuffer* CreateBuffer(const GPUBufferDesc& desc) = 0;
	virtual void DestroyBuffer(GPUBuffer* buffer) = 0;

	virtual GPUImage* CreateImage(const GPUImageDesc& desc) = 0;
	virtual void DestroyImage(GPUImage* image) = 0;

	void UploadStagingData(const void* data, size_t size, GPUBuffer** outBuffer, size_t* outBufferOffset);
	void UploadBuffer(GPUBuffer* dest, size_t size, size_t offset, const void* data);
	void UploadImage(GPUImage* dest, size_t size, const void* data);

	virtual GPUSampler* CreateSampler(const GPUSamplerDesc& desc) = 0;
	virtual void DestroySampler(GPUSampler* sampler) = 0;
	virtual GPUSampler* GetSampler(U32 index) = 0;

	virtual GPUShaderModule* CreateShaderModule(const GPUShaderModuleDesc& desc) = 0;
	virtual void DestroyShaderModule(GPUShaderModule* shader) = 0;

	virtual GPUGraphicsPipeline* CreateGraphicsPipeline(const GPUGraphicsPipelineDesc& desc) = 0;
	virtual void DestroyGraphicsPipeline(GPUGraphicsPipeline* pipeline) = 0;

	virtual void SetSwapchainDesc(GPUSwapchainDesc desc) = 0;
	virtual GPUSwapchainDesc GetSwapchainDesc() = 0;

	Promise GetInitPromise();

protected:
	Promise    m_initPromise;
	GPUBuffer* m_frameUploadBuffer = nullptr;
	size_t     m_frameUploadOffset = 0;
};
