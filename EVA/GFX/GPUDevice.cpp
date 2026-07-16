#include <EVA/GFX/GPUDevice.hpp>
#include <EVA/GFX/GPUDevice_Vulkan.hpp>

bool GPUFormatIsDepth(GPUFormat format) {
	return format >= GPUFormat::DEPTH_FORMAT_START && format <= GPUFormat::DEPTH_FORMAT_END;
}

bool GPUFormatHasStencil(GPUFormat format) {
	return format == GPUFormat::D16_UNORM_S8_UINT || format == GPUFormat::D24_UNORM_S8_UINT || format == GPUFormat::D32_FLOAT_S8_UINT;
}

GPUBlendState GPUBlendModeToBlendState(GPUBlendMode mode) {
	switch (mode) {
		case GPUBlendMode::None:
		case GPUBlendMode::Solid:
			return GPUBlendState{
				.blendEnable      = false,
				.sourceColorBlend = GPUBlendFactor::One,
				.destColorBlend   = GPUBlendFactor::Zero,
				.sourceAlphaBlend = GPUBlendFactor::One,
				.destAlphaBlend   = GPUBlendFactor::Zero,
			};
		case GPUBlendMode::AlphaBlend:
			return GPUBlendState{
				.blendEnable      = true,
				.sourceColorBlend = GPUBlendFactor::SourceAlpha,
				.destColorBlend   = GPUBlendFactor::OneMinusSourceAlpha,
				.sourceAlphaBlend = GPUBlendFactor::One,
				.destAlphaBlend   = GPUBlendFactor::OneMinusSourceAlpha,
			};
		case GPUBlendMode::Add:
			return GPUBlendState{
				.blendEnable      = true,
				.sourceColorBlend = GPUBlendFactor::SourceAlpha,
				.destColorBlend   = GPUBlendFactor::One,
				.sourceAlphaBlend = GPUBlendFactor::Zero,
				.destAlphaBlend   = GPUBlendFactor::One,
			};
		case GPUBlendMode::Multiply:
			return GPUBlendState{
				.blendEnable      = true,
				.sourceColorBlend = GPUBlendFactor::DestColor,
				.destColorBlend   = GPUBlendFactor::Zero,
				.sourceAlphaBlend = GPUBlendFactor::Zero,
				.destAlphaBlend   = GPUBlendFactor::One,
			};
	}
	return {};
}

static GPUDevice* g_gpuDevice = nullptr;

Result GPUDevice::Create(const GPUDeviceInitDesc& desc) {
	if (g_gpuDevice) return Err("GPUDevice is already initialized");

	switch (desc.backend) {
		case GPUBackend::Vulkan: {
			g_gpuDevice = new GPUDevice_Vulkan();
			break;
		}
		default: return Err("Unsupported GPU backend");
	}
	Result initResult = g_gpuDevice->Init(desc);
	if (!initResult) {
		delete g_gpuDevice;
		g_gpuDevice = nullptr;
		return initResult;
	}
	g_gpuDevice->m_frameUploadBuffer = g_gpuDevice->CreateBuffer(GPUBufferDesc{
		.size = FrameUploadBufferSize,
		.usage = GPUBufferUsage_TransferSource | GPUBufferUsage_StorageBuffer,
		.memoryUsage = GPUMemoryUsage::CPUToGPU,
		.bindless = true,
	});
	return Success();
}

void GPUDevice::Shutdown() {
	if (g_gpuDevice && g_gpuDevice->m_frameUploadBuffer)
		g_gpuDevice->DestroyBuffer(g_gpuDevice->m_frameUploadBuffer);
	delete g_gpuDevice;
	g_gpuDevice = nullptr;
}

GPUDevice* GPUDevice::Get() {
	return g_gpuDevice;
}

void GPUDevice::UploadStagingData(const void* data, size_t size, GPUBuffer** outBuffer, size_t* outBufferOffset) {
	size_t alignment = 64;
	size_t offset = (m_frameUploadOffset + alignment - 1) & ~(alignment - 1);
	if (offset + size > FrameUploadBufferSize)
		Fatal("GPUDevice frame upload buffer exhausted (%zu requested, %zu available)", size, FrameUploadBufferSize - offset);

	memcpy((U8*)m_frameUploadBuffer->m_mapped + offset, data, size);
	m_frameUploadOffset = offset + size;
	*outBuffer = m_frameUploadBuffer;
	*outBufferOffset = offset;
}

void GPUDevice::UploadBuffer(GPUBuffer* dest, size_t size, size_t offset, const void* data) {
	GPUBuffer* uploadBuffer = nullptr;
	size_t uploadOffset = 0;
	UploadStagingData(data, size, &uploadBuffer, &uploadOffset);
	GetTransferCommandBuffer()->CopyBuffer(uploadBuffer, dest, {
		.sourceOffset = uploadOffset,
		.destOffset = offset,
		.size = size,
	});
}

void GPUDevice::UploadImage(GPUImage* dest, size_t size, const void* data) {
	GPUBuffer* uploadBuffer = nullptr;
	size_t uploadOffset = 0;
	UploadStagingData(data, size, &uploadBuffer, &uploadOffset);

	GPUCommandBuffer* cmd = GetTransferCommandBuffer();
	cmd->ImageBarrier({
		.image = dest,
		.stateBefore = dest->m_state,
		.stateAfter = GPUImageState::TransferDest,
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
			.stateBefore = GPUImageState::TransferDest,
			.stateAfter = GPUImageState::ShaderRead,
		});
	}
}
