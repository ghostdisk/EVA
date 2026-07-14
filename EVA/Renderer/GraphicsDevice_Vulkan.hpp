#pragma once
#include <EVA/Renderer/GraphicsDevice.hpp>
#include <volk.h>
#include <vector>

typedef struct VmaAllocator_T* VmaAllocator;

namespace GFX {

class CommandBuffer_Vulkan final : public CommandBuffer {
public:
	ECLASS_COMMON();

	virtual void BeginRendering(const RenderingDesc& desc) override;
	virtual void EndRendering(const RenderingDesc& desc) override;

	virtual void BindPipeline(GraphicsPipeline* pipeline) override;
	virtual void BindIndexBuffer(GPUBuffer* buffer, IndexType type, U64 offset = 0) override;
	virtual void PushConstants(GraphicsPipeline* pipeline, U32 size, const void* data) override;

	virtual void Draw(U32 vertexCount, U32 instanceCount = 1, U32 firstVertex = 0, U32 firstInstance = 0) override;
	virtual void DrawIndexed(U32 indexCount, U32 instanceCount = 1, U32 firstIndex = 0, I32 vertexOffset = 0, U32 firstInstance = 0) override;

	virtual void CopyBuffer(GPUBuffer* source, GPUBuffer* destination, const BufferCopy& copy) override;
	virtual void CopyBufferToImage(GPUBuffer* source, Image* destination, const BufferImageCopy& copy) override;
	virtual void ImageBarrier(const GFX::ImageBarrier& barrier) override;
	virtual void GenerateMipmaps(Image* image) override;
};

class GraphicsDevice_Vulkan final : public GraphicsDevice {

private:
	U32                        m_bindlessBufferCount            = 4096;
	U32                        m_bindlessImageCount             = 4096;
	U32                        m_bindlessSamplerCount           = 128;

	VkInstance                 m_instance                       = nullptr;
	VkSurfaceKHR               m_surface                        = nullptr;
	VkPhysicalDevice           m_physicalDevice                 = nullptr;
	VkDevice                   m_device                         = nullptr;
	VkQueue                    m_graphicsQueue                  = nullptr;
	VkSwapchainKHR             m_swapchain                      = nullptr;
	VkSemaphore                m_imageAcquiredSemaphore         = nullptr;
	VkDescriptorSetLayout      m_bindlessDescriptorSetLayout    = nullptr;
	VkDescriptorPool           m_bindlessDescriptorPool         = nullptr;
	VkDescriptorSet            m_bindlessDescriptorSet          = nullptr;
	VkPipelineLayout           m_pipelineLayout                 = nullptr;
	VmaAllocator               m_allocator                      = nullptr;
	BindlessPool<GPUBuffer*>   m_bindlessBuffers                = {};
	BindlessPool<Image*>       m_bindlessImages                 = {};
	BindlessPool<Sampler*>     m_bindlessSamplers               = {};
	U32                        m_graphicsFamily                 = UINT32_MAX;
	VkCommandPool              m_mainCommandPool                = nullptr;
	VkCommandPool              m_transferCommandPool            = nullptr;
	CommandBuffer_Vulkan*      m_mainCommandBuffer              = nullptr;
	CommandBuffer_Vulkan*      m_transferCommandBuffer          = nullptr;
	bool                       m_frameCommandBuffersBegun       = false;
	std::vector<Image*>        m_swapchainImages                = {};
	std::vector<VkSemaphore>   m_renderDoneSemaphores           = {};
	U32                        m_swapchainImageIndex            = 0;
	VkFence                    m_frameFence                     = nullptr;
	bool                       m_needsNewSwapchain              = true;
	SwapchainDesc              m_swapchainDesc                  = {};

public:
	ECLASS_COMMON();

	GraphicsDevice_Vulkan() = default;
	virtual ~GraphicsDevice_Vulkan() override;
	virtual Result Init(const GraphicsDeviceInitDesc& desc) override;

	virtual bool BeginFrame() override;
	virtual void EndFrame() override;

	virtual CommandBuffer* GetMainCommandBuffer() override;
	virtual CommandBuffer* GetTransferCommandBuffer() override;

	virtual Image* GetCurrentBackbuffer() override;

	virtual void WaitIdle() override;

	virtual CommandBuffer* BeginImmediateCommandBuffer() override;
	virtual void FlushImmediateCommandBuffer(CommandBuffer* cmd) override;

	virtual GPUBuffer* CreateGPUBuffer(const GPUBufferDesc& desc) override;
	virtual void DestroyGPUBuffer(GPUBuffer* buffer) override;

	virtual Image* CreateImage(const ImageDesc& desc) override;
	virtual void DestroyImage(Image* image) override;

	virtual Sampler* CreateSampler(const SamplerDesc& desc) override;
	virtual void DestroySampler(Sampler* sampler) override;
	virtual Sampler* GetSampler(U32 index) override;

	virtual ShaderModule* CreateShaderModule(const ShaderModuleDesc& desc) override;
	virtual void DestroyShaderModule(ShaderModule* shader) override;

	virtual GraphicsPipeline* CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
	virtual void DestroyGraphicsPipeline(GraphicsPipeline* pipeline) override;

	virtual void SetSwapchainDesc(SwapchainDesc desc) override;
	virtual SwapchainDesc GetSwapchainDesc() override;

private:
	Result CreateCommandPool(VkCommandPool* outCommandPool);
	Result CreateCommandBuffer(VkCommandPool commandPool, CommandBuffer_Vulkan** outCommandBuffer);
	Result CreateFence(bool signaled, VkFence* outFence);
	Result BeginFrameCommandBuffers();
	Result CreateSwapchain();
	void   DestroySwapchain();
	Result CreateSemaphore(VkSemaphore* outSemaphore);
};

}
