#include <EVA/Renderer/GraphicsDevice.hpp>
#include <EVA/Renderer/GraphicsDevice_Vulkan.hpp>

namespace GFX {

static GraphicsDevice* g_graphicsDevice = nullptr;

Result GraphicsDevice::Create(const GraphicsDeviceInitDesc& desc) {
	if (g_graphicsDevice) return Err("GraphicsDevice is already initialized");

	switch (desc.api) {
		case GraphicsAPI::Vulkan: {
			g_graphicsDevice = new GraphicsDevice_Vulkan();
			break;
		}
		default: return Err("Unsupported graphics API");
	}
	Result initResult = g_graphicsDevice->Init(desc);
	if (!initResult) {
		delete g_graphicsDevice;
		g_graphicsDevice = nullptr;
		return initResult;
	}
	g_graphicsDevice->m_frameUploadBuffer = g_graphicsDevice->CreateGPUBuffer(GPUBufferDesc{
		.size = FrameUploadBufferSize,
		.usage = GPUBufferUsage_TransferSource | GPUBufferUsage_StorageBuffer,
		.memoryUsage = MemoryUsage::CPUToGPU,
		.bindless = true,
	});
	return Success();
}

void GraphicsDevice::Shutdown() {
	if (g_graphicsDevice && g_graphicsDevice->m_frameUploadBuffer)
		g_graphicsDevice->DestroyGPUBuffer(g_graphicsDevice->m_frameUploadBuffer);
	delete g_graphicsDevice;
	g_graphicsDevice = nullptr;
}

GraphicsDevice* GraphicsDevice::Get() {
	return g_graphicsDevice;
}

bool GraphicsDevice::BeginFrame() {
	// TODO: Move m_frameUploadOffset to Renderer, and get rid of this Impl.
	m_frameUploadOffset = 0;
	return BeginFrameImpl();
}

void GraphicsDevice::UploadFrameData(const void* data, size_t size, GPUBuffer** outBuffer, size_t* outBufferOffset) {
	constexpr size_t alignment = 448;
	size_t offset = (m_frameUploadOffset + alignment - 1) & ~(alignment - 1);
	if (offset + size > FrameUploadBufferSize)
		Fatal("GraphicsDevice frame upload buffer exhausted (%zu requested, %zu available)", size, FrameUploadBufferSize - offset);

	memcpy((U8*)m_frameUploadBuffer->m_mapped + offset, data, size);
	m_frameUploadOffset = offset + size;
	*outBuffer = m_frameUploadBuffer;
	*outBufferOffset = offset;
}

CommandBuffer* GraphicsDevice::BeginImmediateCommandBuffer() {
	CommandBuffer* cmd = CreateCommandBuffer();
	BeginCommandBuffer(cmd);
	return cmd;
}

void GraphicsDevice::FlushImmediateCommandBuffer(CommandBuffer* cmd) {
	EndCommandBuffer(cmd);
	Fence* fence = CreateFence();
	CommandBuffer* commandBuffers[] = { cmd };
	Submit(SubmitDesc{
		.commandBufferCount = 1,
		.commandBuffers = commandBuffers,
		.fence = fence,
	});
	WaitForFence(fence);
	DestroyFence(fence);
	DestroyCommandBuffer(cmd);
}

void GraphicsDevice::UploadBuffer(GPUBuffer* dest, size_t size, size_t offset, const void* data) {
	GPUBuffer* uploadBuffer = nullptr;
	size_t uploadOffset = 0;
	UploadFrameData(data, size, &uploadBuffer, &uploadOffset);
	GetTransferCommandBuffer()->CopyBuffer(uploadBuffer, dest, BufferCopy{
		.sourceOffset = uploadOffset,
		.destOffset = offset,
		.size = size,
	});
}

void GraphicsDevice::UploadImage(Image* dest, size_t size, const void* data) {
	GPUBuffer* uploadBuffer = nullptr;
	size_t uploadOffset = 0;
	UploadFrameData(data, size, &uploadBuffer, &uploadOffset);

	CommandBuffer* cmd = GetTransferCommandBuffer();
	cmd->ImageBarrier(GFX::ImageBarrier{
		.image = dest,
		.stateBefore = dest->m_state,
		.stateAfter = ImageState::TransferDest,
		.mipCount = dest->m_mipCount,
	});
	cmd->CopyBufferToImage(uploadBuffer, dest, BufferImageCopy{
		.bufferOffset = uploadOffset,
		.width = dest->m_width,
		.height = dest->m_height,
	});
	if (dest->m_mipCount > 1) {
		cmd->GenerateMipmaps(dest);
	} else {
		cmd->ImageBarrier(GFX::ImageBarrier{
			.image = dest,
			.stateBefore = ImageState::TransferDest,
			.stateAfter = ImageState::ShaderRead,
		});
	}
}

}
