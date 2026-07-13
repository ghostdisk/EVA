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
public:
	ECLASS_COMMON();

	GraphicsDevice_Vulkan() = default;
	virtual ~GraphicsDevice_Vulkan() override;
	virtual Result Init(const GraphicsDeviceInitDesc& desc) override;

	virtual void EndFrame() override;

	virtual CommandBuffer* GetMainCommandBuffer() override;
	virtual CommandBuffer* GetTransferCommandBuffer() override;

	virtual Image* GetCurrentBackbuffer() override;

	virtual void SetVSync(bool enabled) override;
	virtual void WaitIdle() override;

	virtual CommandBuffer* CreateCommandBuffer() override;
	virtual void DestroyCommandBuffer(CommandBuffer* cmd) override;
	virtual void BeginCommandBuffer(CommandBuffer* cmd) override;
	virtual void EndCommandBuffer(CommandBuffer* cmd) override;

	virtual Fence* CreateFence(bool signaled = false) override;
	virtual void DestroyFence(Fence* fence) override;
	virtual void WaitForFence(Fence* fence) override;
	virtual void ResetFence(Fence* fence) override;

	virtual void Submit(const SubmitDesc& desc) override;

	virtual GPUBuffer* CreateGPUBuffer(const GPUBufferDesc& desc) override;
	virtual void DestroyGPUBuffer(GPUBuffer* buffer) override;

	virtual Image* CreateImage(const ImageDesc& desc) override;
	virtual void DestroyImage(Image* image) override;

	virtual Sampler* CreateSampler(const SamplerDesc& desc) override;
	virtual void DestroySampler(Sampler* sampler) override;

	virtual ShaderModule* CreateShaderModule(const ShaderModuleDesc& desc) override;
	virtual void DestroyShaderModule(ShaderModule* shader) override;

	virtual GraphicsPipeline* CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
	virtual void DestroyGraphicsPipeline(GraphicsPipeline* pipeline) override;

protected:
	virtual bool BeginFrameImpl() override;

private:
	Result CreateCommandPool(VkCommandPool* outCommandPool);
	Result CreateCommandBuffer(VkCommandPool commandPool, CommandBuffer_Vulkan** outCommandBuffer);
	Result BeginFrameCommandBuffers();
	Result CreateSwapchain(const GraphicsDeviceInitDesc& desc);
	Result CreateSemaphore(VkSemaphore* outSemaphore);
	Result CreateBindlessResources();

	VkInstance       m_instance       = nullptr;
	VkSurfaceKHR     m_surface        = nullptr;
	VkPhysicalDevice m_physicalDevice = nullptr;
	VkDevice         m_device         = nullptr;
	VkQueue          m_graphicsQueue  = nullptr;
	VkSwapchainKHR   m_swapchain      = nullptr;
	VkSemaphore      m_imageAcquiredSemaphore = nullptr;
	VkDescriptorSetLayout m_bindlessDescriptorSetLayout = nullptr;
	VkDescriptorPool m_bindlessDescriptorPool = nullptr;
	VkDescriptorSet  m_bindlessDescriptorSet = nullptr;
	VkPipelineLayout m_pipelineLayout = nullptr;
	VmaAllocator     m_allocator      = nullptr;
	U32              m_bindlessBufferCount = 0;
	U32              m_bindlessImageCount = 0;
	U32              m_bindlessSamplerCount = 0;
	BindlessIndexAllocator m_bindlessBufferIndices;
	BindlessIndexAllocator m_bindlessImageIndices;
	BindlessIndexAllocator m_bindlessSamplerIndices;
	U32              m_graphicsFamily = UINT32_MAX;
	VkCommandPool    m_mainCommandPool = nullptr;
	VkCommandPool    m_transferCommandPool = nullptr;
	CommandBuffer_Vulkan* m_mainCommandBuffer = nullptr;
	CommandBuffer_Vulkan* m_transferCommandBuffer = nullptr;
	bool             m_frameCommandBuffersBegun = false;
	std::vector<Image*> m_swapchainImages;
	std::vector<VkSemaphore> m_renderDoneSemaphores;
	U32              m_swapchainImageIndex = 0;
	Fence*           m_frameFence = nullptr;
};

}
