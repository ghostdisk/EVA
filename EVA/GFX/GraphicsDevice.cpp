#include <EVA/GFX/GraphicsDevice.hpp>
#include <EVA/GFX/GraphicsDevice_Vulkan.hpp>

bool FormatIsDepth(Format format) {
	return format >= Format::DEPTH_FORMAT_START && format <= Format::DEPTH_FORMAT_END;
}

bool FormatHasStencil(Format format) {
	return format == Format::D16_UNORM_S8_UINT || format == Format::D24_UNORM_S8_UINT || format == Format::D32_FLOAT_S8_UINT;
}

BlendState BlendModeToBlendState(BlendMode mode) {
	switch (mode) {
		case BlendMode::None:
		case BlendMode::Solid:
			return BlendState{
				.blendEnable      = false,
				.sourceColorBlend = BlendFactor::One,
				.destColorBlend   = BlendFactor::Zero,
				.sourceAlphaBlend = BlendFactor::One,
				.destAlphaBlend   = BlendFactor::Zero,
			};
		case BlendMode::AlphaBlend:
			return BlendState{
				.blendEnable      = true,
				.sourceColorBlend = BlendFactor::SourceAlpha,
				.destColorBlend   = BlendFactor::OneMinusSourceAlpha,
				.sourceAlphaBlend = BlendFactor::One,
				.destAlphaBlend   = BlendFactor::OneMinusSourceAlpha,
			};
		case BlendMode::Add:
			return BlendState{
				.blendEnable      = true,
				.sourceColorBlend = BlendFactor::SourceAlpha,
				.destColorBlend   = BlendFactor::One,
				.sourceAlphaBlend = BlendFactor::Zero,
				.destAlphaBlend   = BlendFactor::One,
			};
		case BlendMode::Multiply:
			return BlendState{
				.blendEnable      = true,
				.sourceColorBlend = BlendFactor::DestColor,
				.destColorBlend   = BlendFactor::Zero,
				.sourceAlphaBlend = BlendFactor::Zero,
				.destAlphaBlend   = BlendFactor::One,
			};
	}
	return {};
}

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

void GraphicsDevice::UploadStagingData(const void* data, size_t size, GPUBuffer** outBuffer, size_t* outBufferOffset) {
	size_t alignment = 64;
	size_t offset = (m_frameUploadOffset + alignment - 1) & ~(alignment - 1);
	if (offset + size > FrameUploadBufferSize)
		Fatal("GraphicsDevice frame upload buffer exhausted (%zu requested, %zu available)", size, FrameUploadBufferSize - offset);

	memcpy((U8*)m_frameUploadBuffer->m_mapped + offset, data, size);
	m_frameUploadOffset = offset + size;
	*outBuffer = m_frameUploadBuffer;
	*outBufferOffset = offset;
}

void GraphicsDevice::UploadBuffer(GPUBuffer* dest, size_t size, size_t offset, const void* data) {
	GPUBuffer* uploadBuffer = nullptr;
	size_t uploadOffset = 0;
	UploadStagingData(data, size, &uploadBuffer, &uploadOffset);
	GetTransferCommandBuffer()->CopyBuffer(uploadBuffer, dest, {
		.sourceOffset = uploadOffset,
		.destOffset = offset,
		.size = size,
	});
}

void GraphicsDevice::UploadImage(Image* dest, size_t size, const void* data) {
	GPUBuffer* uploadBuffer = nullptr;
	size_t uploadOffset = 0;
	UploadStagingData(data, size, &uploadBuffer, &uploadOffset);

	CommandBuffer* cmd = GetTransferCommandBuffer();
	cmd->ImageBarrier({
		.image = dest,
		.stateBefore = dest->m_state,
		.stateAfter = ImageState::TransferDest,
		.mipCount = dest->m_mipCount,
	});
	cmd->CopyBufferToImage(uploadBuffer, dest, {
		.bufferOffset = uploadOffset,
		.width = dest->m_width,
		.height = dest->m_height,
	});
	if (dest->m_mipCount > 1) {
		cmd->GenerateMipmaps(dest);
	} else {
		cmd->ImageBarrier({
			.image = dest,
			.stateBefore = ImageState::TransferDest,
			.stateAfter = ImageState::ShaderRead,
		});
	}
}
