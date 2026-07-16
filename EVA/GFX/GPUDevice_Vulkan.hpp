#pragma once
#include <EVA/GFX/GPUDevice.hpp>
#include <volk.h>

typedef struct VmaAllocator_T* VmaAllocator;

class GPUCommandBuffer_Vulkan final : public GPUCommandBuffer {
public:
	ECLASS_COMMON();

	virtual void BeginRendering(const GPURenderingDesc& desc) override;
	virtual void EndRendering(const GPURenderingDesc& desc) override;

	virtual void BindPipeline(GPUGraphicsPipeline* pipeline) override;
	virtual void BindIndexBuffer(GPUBuffer* buffer, GPUIndexType type, U64 offset = 0) override;
	virtual void PushConstants(GPUGraphicsPipeline* pipeline, U32 size, const void* data) override;

	virtual void Draw(U32 vertexCount, U32 instanceCount = 1, U32 firstVertex = 0, U32 firstInstance = 0) override;
	virtual void DrawIndexed(U32 indexCount, U32 instanceCount = 1, U32 firstIndex = 0, I32 vertexOffset = 0, U32 firstInstance = 0) override;

	virtual void CopyBuffer(GPUBuffer* source, GPUBuffer* destination, const GPUBufferCopyDesc& copy) override;
	virtual void CopyBufferToImage(GPUBuffer* source, GPUImage* destination, const GPUBufferImageCopyDesc& copy) override;
	virtual void ImageBarrier(const GPUImageBarrierDesc& barrier) override;
	virtual void GenerateMipmaps(GPUImage* image) override;
};

class GPUDevice_Vulkan final : public GPUDevice {

private:
	VkInstance                      m_instance                          = nullptr;
	VkDebugUtilsMessengerEXT        m_debugMessenger                    = nullptr;
	VkSurfaceKHR                    m_surface                           = nullptr;
	VkPhysicalDevice                m_physicalDevice                    = nullptr;
	VkDevice                        m_device                            = nullptr;
	VkQueue                         m_graphicsQueue                     = nullptr;
	VkSwapchainKHR                  m_swapchain                         = nullptr;
	VkSemaphore                     m_imageAcquiredSemaphore            = nullptr;
	VkDescriptorSetLayout           m_bindlessDescriptorSetLayout       = nullptr;
	VkDescriptorPool                m_bindlessDescriptorPool            = nullptr;
	VkDescriptorSet                 m_bindlessDescriptorSet             = nullptr;
	VkPipelineLayout                m_pipelineLayout                    = nullptr;
	VmaAllocator                    m_allocator                         = nullptr;
	GPUResourcePool<GPUBuffer*>     m_buffers                           = {};
	GPUResourcePool<GPUGraphicsPipeline*> m_pipelines                   = {};
	GPUResourcePool<GPUImage*>      m_images                            = {};
	GPUResourcePool<GPUSampler*>    m_samplers                          = {};
	U32                             m_graphicsFamily                    = UINT32_MAX;
	VkCommandPool                   m_mainCommandPool                   = nullptr;
	VkCommandPool                   m_transferCommandPool               = nullptr;
	GPUCommandBuffer_Vulkan         m_mainCommandBuffer                 = {};
	GPUCommandBuffer_Vulkan         m_transferCommandBuffer             = {};
	bool                            m_transfersBegun                    = false;
	bool                            m_frameBegun                        = false;
	Vector<GPUImage*>               m_swapchainImages                   = {};
	Vector<VkSemaphore>             m_renderDoneSemaphores              = {};
	U32                             m_swapchainImageIndex               = 0;
	VkFence                         m_frameFence                        = nullptr;
	bool                            m_needsNewSwapchain                 = true;
	GPUSwapchainDesc                m_swapchainDesc                     = {};

public:
	ECLASS_COMMON();

	GPUDevice_Vulkan() = default;
	virtual ~GPUDevice_Vulkan() override;
	virtual Result Init(const GPUDeviceInitDesc& desc) override;

	virtual void BeginTransfers() override;
	virtual bool BeginFrame() override;
	virtual void EndFrame() override;

	virtual GPUCommandBuffer* GetMainCommandBuffer() override;
	virtual GPUCommandBuffer* GetTransferCommandBuffer() override;

	virtual GPUImage* GetCurrentBackbuffer() override;

	virtual void WaitIdle() override;

	virtual GPUCommandBuffer* BeginImmediateCommandBuffer() override;
	virtual void FlushImmediateCommandBuffer(GPUCommandBuffer* cmd) override;

	virtual GPUBuffer* CreateBuffer(const GPUBufferDesc& desc) override;
	virtual void DestroyBuffer(GPUBuffer* buffer) override;

	virtual GPUImage* CreateImage(const GPUImageDesc& desc) override;
	virtual void DestroyImage(GPUImage* image) override;

	virtual GPUSampler* CreateSampler(const GPUSamplerDesc& desc) override;
	virtual void DestroySampler(GPUSampler* sampler) override;
	virtual GPUSampler* GetSampler(U32 index) override;

	virtual GPUShaderModule* CreateShaderModule(const GPUShaderModuleDesc& desc) override;
	virtual void DestroyShaderModule(GPUShaderModule* shader) override;

	virtual GPUGraphicsPipeline* CreateGraphicsPipeline(const GPUGraphicsPipelineDesc& desc) override;
	virtual void DestroyGraphicsPipeline(GPUGraphicsPipeline* pipeline) override;

	virtual void SetSwapchainDesc(GPUSwapchainDesc desc) override;
	virtual GPUSwapchainDesc GetSwapchainDesc() override;

private:
	Result CreateCommandPool(VkCommandPool* outCommandPool);
	Result InitCommandBuffer(VkCommandPool commandPool, GPUCommandBuffer_Vulkan* outCommandBuffer);
	Result CreateFence(bool signaled, VkFence* outFence);
	Result CreateSwapchain();
	void   DestroySwapchain();
	Result CreateSemaphore(VkSemaphore* outSemaphore);
};
