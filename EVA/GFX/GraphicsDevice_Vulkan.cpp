#include <volk.h>
#include <EVA/GFX/GraphicsDevice_Vulkan.hpp>
#include <EVA/Core/Common.hpp>
#include <SDL3/SDL_vulkan.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#define VMA_LEAK_LOG_FORMAT(format, ...) do { \
        printf((format), __VA_ARGS__); \
        printf("\n"); \
    } while(false)
#include <Vendor/vk_mem_alloc.h>

#define VK_TRY(expr) \
	do { \
		VkResult res = (expr); \
		if (res != VK_SUCCESS) { \
			return Err("Vulkan Error %u at %s:%d", (U32)res, __FILE__, __LINE__); \
		} \
	} while (0)


static bool HasInstanceLayer(String name) {
	U32 count = 0;
	vkEnumerateInstanceLayerProperties(&count, nullptr);
	Vector<VkLayerProperties> layers(count);
	vkEnumerateInstanceLayerProperties(&count, layers.data());
	for (const VkLayerProperties& layer : layers)
		if (name == layer.layerName)
			return true;
	return false;
}

struct FormatMapping { Format format; VkFormat vkFormat; };
#define FORMAT_MAP(format, vkFormat) { Format::format, VK_FORMAT_##vkFormat }
static constexpr FormatMapping g_formatMappings[] = {
	FORMAT_MAP(None, UNDEFINED),
	FORMAT_MAP(R8_UNORM, R8_UNORM), FORMAT_MAP(R8_SNORM, R8_SNORM), FORMAT_MAP(R8_UINT, R8_UINT), FORMAT_MAP(R8_SINT, R8_SINT), FORMAT_MAP(R8_SRGB, R8_SRGB),
	FORMAT_MAP(RG8_UNORM, R8G8_UNORM), FORMAT_MAP(RG8_SNORM, R8G8_SNORM), FORMAT_MAP(RG8_UINT, R8G8_UINT), FORMAT_MAP(RG8_SINT, R8G8_SINT), FORMAT_MAP(RG8_SRGB, R8G8_SRGB),
	FORMAT_MAP(RGB8_UNORM, R8G8B8_UNORM), FORMAT_MAP(RGB8_SNORM, R8G8B8_SNORM), FORMAT_MAP(RGB8_UINT, R8G8B8_UINT), FORMAT_MAP(RGB8_SINT, R8G8B8_SINT), FORMAT_MAP(RGB8_SRGB, R8G8B8_SRGB),
	FORMAT_MAP(BGR8_UNORM, B8G8R8_UNORM), FORMAT_MAP(BGR8_SNORM, B8G8R8_SNORM), FORMAT_MAP(BGR8_UINT, B8G8R8_UINT), FORMAT_MAP(BGR8_SINT, B8G8R8_SINT), FORMAT_MAP(BGR8_SRGB, B8G8R8_SRGB),
	FORMAT_MAP(RGBA8_UNORM, R8G8B8A8_UNORM), FORMAT_MAP(RGBA8_SNORM, R8G8B8A8_SNORM), FORMAT_MAP(RGBA8_UINT, R8G8B8A8_UINT), FORMAT_MAP(RGBA8_SINT, R8G8B8A8_SINT), FORMAT_MAP(RGBA8_SRGB, R8G8B8A8_SRGB),
	FORMAT_MAP(BGRA8_UNORM, B8G8R8A8_UNORM), FORMAT_MAP(BGRA8_SNORM, B8G8R8A8_SNORM), FORMAT_MAP(BGRA8_UINT, B8G8R8A8_UINT), FORMAT_MAP(BGRA8_SINT, B8G8R8A8_SINT), FORMAT_MAP(BGRA8_SRGB, B8G8R8A8_SRGB),
	FORMAT_MAP(R16_UNORM, R16_UNORM), FORMAT_MAP(R16_SNORM, R16_SNORM), FORMAT_MAP(R16_UINT, R16_UINT), FORMAT_MAP(R16_SINT, R16_SINT), FORMAT_MAP(R16_FLOAT, R16_SFLOAT),
	FORMAT_MAP(RG16_UNORM, R16G16_UNORM), FORMAT_MAP(RG16_SNORM, R16G16_SNORM), FORMAT_MAP(RG16_UINT, R16G16_UINT), FORMAT_MAP(RG16_SINT, R16G16_SINT), FORMAT_MAP(RG16_FLOAT, R16G16_SFLOAT),
	FORMAT_MAP(RGB16_UNORM, R16G16B16_UNORM), FORMAT_MAP(RGB16_SNORM, R16G16B16_SNORM), FORMAT_MAP(RGB16_UINT, R16G16B16_UINT), FORMAT_MAP(RGB16_SINT, R16G16B16_SINT), FORMAT_MAP(RGB16_FLOAT, R16G16B16_SFLOAT),
	FORMAT_MAP(RGBA16_UNORM, R16G16B16A16_UNORM), FORMAT_MAP(RGBA16_SNORM, R16G16B16A16_SNORM), FORMAT_MAP(RGBA16_UINT, R16G16B16A16_UINT), FORMAT_MAP(RGBA16_SINT, R16G16B16A16_SINT), FORMAT_MAP(RGBA16_FLOAT, R16G16B16A16_SFLOAT),
	FORMAT_MAP(R32_UINT, R32_UINT), FORMAT_MAP(R32_SINT, R32_SINT), FORMAT_MAP(R32_FLOAT, R32_SFLOAT),
	FORMAT_MAP(RG32_UINT, R32G32_UINT), FORMAT_MAP(RG32_SINT, R32G32_SINT), FORMAT_MAP(RG32_FLOAT, R32G32_SFLOAT),
	FORMAT_MAP(RGB32_UINT, R32G32B32_UINT), FORMAT_MAP(RGB32_SINT, R32G32B32_SINT), FORMAT_MAP(RGB32_FLOAT, R32G32B32_SFLOAT),
	FORMAT_MAP(RGBA32_UINT, R32G32B32A32_UINT), FORMAT_MAP(RGBA32_SINT, R32G32B32A32_SINT), FORMAT_MAP(RGBA32_FLOAT, R32G32B32A32_SFLOAT),
	FORMAT_MAP(D16_UNORM, D16_UNORM), FORMAT_MAP(D32_FLOAT, D32_SFLOAT), FORMAT_MAP(D16_UNORM_S8_UINT, D16_UNORM_S8_UINT), FORMAT_MAP(D24_UNORM_S8_UINT, D24_UNORM_S8_UINT), FORMAT_MAP(D32_FLOAT_S8_UINT, D32_SFLOAT_S8_UINT),
};
#undef FORMAT_MAP

static VkFormat ToVkFormat(Format format) {
	for (const FormatMapping& mapping : g_formatMappings)
		if (mapping.format == format) return mapping.vkFormat;
	return VK_FORMAT_UNDEFINED;
}

static Format FromVkFormat(VkFormat format) {
	for (const FormatMapping& mapping : g_formatMappings)
		if (mapping.vkFormat == format) return mapping.format;
	return Format::None;
}

static VkImageUsageFlags ToVkImageUsage(ImageUsage usage) {
	VkImageUsageFlags result = 0;
	if (usage & ImageUsage_TransferSource) result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (usage & ImageUsage_TransferDest) result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (usage & ImageUsage_Sampled) result |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (usage & ImageUsage_Storage) result |= VK_IMAGE_USAGE_STORAGE_BIT;
	if (usage & ImageUsage_ColorAttachment) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (usage & ImageUsage_DepthAttachment) result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	return result;
}

static VkAccessFlags2 ImageStateToAccessFlags(ImageState state) {
	switch (state) {
		case ImageState::Undefined:       return VK_ACCESS_2_NONE;
		case ImageState::General:         return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		case ImageState::TransferSource:  return VK_ACCESS_2_TRANSFER_READ_BIT;
		case ImageState::TransferDest:    return VK_ACCESS_2_TRANSFER_WRITE_BIT;
		case ImageState::ColorAttachment: return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		case ImageState::DepthAttachment: return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case ImageState::DepthReadOnly:   return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;
		case ImageState::ShaderRead:      return VK_ACCESS_2_SHADER_READ_BIT;
		case ImageState::Present:         return VK_ACCESS_2_NONE;
	}
	return VK_ACCESS_2_NONE;
}

static VkImageLayout ImageStateToImageLayout(ImageState state) {
	switch (state) {
		case ImageState::Undefined:       return VK_IMAGE_LAYOUT_UNDEFINED;
		case ImageState::General:         return VK_IMAGE_LAYOUT_GENERAL;
		case ImageState::TransferSource:  return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case ImageState::TransferDest:    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case ImageState::ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case ImageState::DepthAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case ImageState::DepthReadOnly:   return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		case ImageState::ShaderRead:      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case ImageState::Present:         return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	return VK_IMAGE_LAYOUT_UNDEFINED;
}

static VkPipelineStageFlags2 ImageStateToPipelineStage(ImageState state) {
	switch (state) {
		case ImageState::Undefined:       return VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		case ImageState::General:         return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		case ImageState::TransferSource:
		case ImageState::TransferDest:    return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		case ImageState::ColorAttachment: return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		case ImageState::DepthAttachment:
		case ImageState::DepthReadOnly:   return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		case ImageState::ShaderRead:      return VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		case ImageState::Present:         return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	}
	return VK_PIPELINE_STAGE_2_NONE;
}

static VkFilter ToVkFilter(Filter filter) {
	return filter == Filter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

static VkSamplerMipmapMode ToVkMipmapMode(MipmapMode mode) {
	return mode == MipmapMode::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

static VkSamplerAddressMode ToVkAddressMode(AddressMode mode) {
	switch (mode) {
		case AddressMode::Repeat:            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case AddressMode::MirroredRepeat:    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case AddressMode::ClampToEdge:       return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case AddressMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
	}
	return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

static VkAttachmentLoadOp ToVkLoadOp(LoadOp op) {
	switch (op) {
		case LoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
		case LoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}
	return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

static VkAttachmentStoreOp ToVkStoreOp(StoreOp op) {
	switch (op) {
		case StoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
		case StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
	return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

static VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveTopology topology) {
	switch (topology) {
		case PrimitiveTopology::PointList:                   return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case PrimitiveTopology::LineList:                    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case PrimitiveTopology::LineStrip:                   return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case PrimitiveTopology::TriangleList:                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case PrimitiveTopology::TriangleStrip:               return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		case PrimitiveTopology::TriangleFan:                 return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
		case PrimitiveTopology::LineListWithAdjacency:       return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
		case PrimitiveTopology::LineStripWithAdjacency:      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
		case PrimitiveTopology::TriangleListWithAdjacency:   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
		case PrimitiveTopology::TriangleStripWithAdjacency:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
		case PrimitiveTopology::PatchList:                   return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	}
	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

static VkPolygonMode ToVkPolygonMode(PolygonMode mode) {
	switch (mode) {
		case PolygonMode::Fill:  return VK_POLYGON_MODE_FILL;
		case PolygonMode::Line:  return VK_POLYGON_MODE_LINE;
		case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
	}
	return VK_POLYGON_MODE_FILL;
}

static VkCullModeFlags ToVkCullMode(CullMode mode) {
	switch (mode) {
		case CullMode::None:         return VK_CULL_MODE_NONE;
		case CullMode::Front:        return VK_CULL_MODE_FRONT_BIT;
		case CullMode::Back:         return VK_CULL_MODE_BACK_BIT;
		case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
	}
	return VK_CULL_MODE_NONE;
}

static VkFrontFace ToVkFrontFace(FrontFace face) {
	switch (face) {
		case FrontFace::Clockwise:        return VK_FRONT_FACE_CLOCKWISE;
		case FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
	}
	return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

static VkCompareOp ToVkCompareOp(CompareOp op) {
	switch (op) {
		case CompareOp::Never:        return VK_COMPARE_OP_NEVER;
		case CompareOp::Less:         return VK_COMPARE_OP_LESS;
		case CompareOp::Equal:        return VK_COMPARE_OP_EQUAL;
		case CompareOp::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOp::Greater:      return VK_COMPARE_OP_GREATER;
		case CompareOp::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
		case CompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOp::Always:       return VK_COMPARE_OP_ALWAYS;
	}
	return VK_COMPARE_OP_ALWAYS;
}

static VkBlendFactor ToVkBlendFactor(BlendFactor factor) {
	switch (factor) {
		case BlendFactor::Zero:                     return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::One:                      return VK_BLEND_FACTOR_ONE;
		case BlendFactor::SourceColor:              return VK_BLEND_FACTOR_SRC_COLOR;
		case BlendFactor::OneMinusSourceColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BlendFactor::DestColor:                return VK_BLEND_FACTOR_DST_COLOR;
		case BlendFactor::OneMinusDestColor:        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BlendFactor::SourceAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::OneMinusSourceAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::DestAlpha:                return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::OneMinusDestAlpha:        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case BlendFactor::ConstantColor:            return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case BlendFactor::OneMinusConstantColor:    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case BlendFactor::ConstantAlpha:            return VK_BLEND_FACTOR_CONSTANT_ALPHA;
		case BlendFactor::OneMinusConstantAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		case BlendFactor::SourceAlphaSaturate:      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		case BlendFactor::Source1Color:             return VK_BLEND_FACTOR_SRC1_COLOR;
		case BlendFactor::OneMinusSource1Color:     return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
		case BlendFactor::Source1Alpha:             return VK_BLEND_FACTOR_SRC1_ALPHA;
		case BlendFactor::OneMinusSource1Alpha:     return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
	}
	return VK_BLEND_FACTOR_ONE;
}

static VkBlendOp ToVkBlendOp(BlendOp op) {
	switch (op) {
		case BlendOp::Add:             return VK_BLEND_OP_ADD;
		case BlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
		case BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendOp::Min:             return VK_BLEND_OP_MIN;
		case BlendOp::Max:             return VK_BLEND_OP_MAX;
	}
	return VK_BLEND_OP_ADD;
}

static I64 ScorePhysicalDevice(VkPhysicalDevice physicalDevice, U32* outGraphicsFamily) {
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	if (properties.apiVersion < VK_API_VERSION_1_3) return -1;

	U32 queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
	U32 graphicsFamily = UINT32_MAX;
	for (U32 i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsFamily = i;
			break;
		}
	}
	if (graphicsFamily == UINT32_MAX) return -1;

	VkPhysicalDeviceVulkan12Features features12{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
	};
	VkPhysicalDeviceVulkan13Features features13{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &features12,
	};
	VkPhysicalDeviceFeatures2 features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &features13,
	};
	vkGetPhysicalDeviceFeatures2(physicalDevice, &features);
	if (!features.features.samplerAnisotropy ||
		!features12.shaderSampledImageArrayNonUniformIndexing ||
		!features12.shaderStorageBufferArrayNonUniformIndexing ||
		!features12.descriptorBindingSampledImageUpdateAfterBind ||
		!features12.descriptorBindingStorageBufferUpdateAfterBind ||
		!features12.descriptorBindingPartiallyBound ||
		!features12.runtimeDescriptorArray ||
		!features12.scalarBlockLayout ||
		!features13.synchronization2 ||
		!features13.dynamicRendering)
		return -1;

	I64 score = properties.limits.maxImageDimension2D;
	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
	else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 1000;
	*outGraphicsFamily = graphicsFamily;
	return score;
}

Result GraphicsDevice_Vulkan::CreateCommandPool(VkCommandPool* outCommandPool) {
	VkCommandPoolCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_graphicsFamily,
	};
	VK_TRY(vkCreateCommandPool(m_device, &createInfo, nullptr, outCommandPool));
	return Success();
}

Result GraphicsDevice_Vulkan::CreateCommandBuffer(VkCommandPool commandPool, CommandBuffer_Vulkan** outCommandBuffer) {
	VkCommandBufferAllocateInfo allocateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkCommandBuffer vkCmd;
	VK_TRY(vkAllocateCommandBuffers(m_device, &allocateInfo, &vkCmd));
	CommandBuffer_Vulkan* commandBuffer = new CommandBuffer_Vulkan();
	commandBuffer->m_vk = vkCmd;
	*outCommandBuffer = commandBuffer;
	return Success();
}

Result GraphicsDevice_Vulkan::BeginFrameCommandBuffers() {
	if (m_frameCommandBuffersBegun) return Success();

	m_frameUploadOffset = 0;

	VK_TRY(vkResetCommandPool(m_device, m_mainCommandPool, 0));
	VK_TRY(vkResetCommandPool(m_device, m_transferCommandPool, 0));
	VkCommandBufferBeginInfo beginInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_TRY(vkBeginCommandBuffer(m_mainCommandBuffer->m_vk, &beginInfo));
	VK_TRY(vkBeginCommandBuffer(m_transferCommandBuffer->m_vk, &beginInfo));
	vkCmdBindDescriptorSets(
		m_mainCommandBuffer->m_vk,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_pipelineLayout,
		0,
		1,
		&m_bindlessDescriptorSet,
		0,
		nullptr);
	m_frameCommandBuffersBegun = true;
	return Success();
}

void GraphicsDevice_Vulkan::DestroySwapchain() {
	m_needsNewSwapchain = true;

	for (Image* image : m_swapchainImages)
		DestroyImage(image);
	m_swapchainImages.clear();

	for (VkSemaphore semaphore : m_renderDoneSemaphores)
		vkDestroySemaphore(m_device, semaphore, nullptr);
	m_renderDoneSemaphores.clear();

	if (m_swapchain) {
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		m_swapchain = nullptr;
	}
}

Result GraphicsDevice_Vulkan::CreateSwapchain() {
	assert(m_needsNewSwapchain);

	VkSurfaceCapabilitiesKHR capabilities;
	VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities));

	VkExtent2D extent = capabilities.currentExtent;
	if (extent.width == 0 || extent.height == 0) {
		// m_needsNewSwapchain remains true.
		return Success();
	}

	U32 formatCount = 0;
	VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr));
	if (!formatCount) {
		return Err("Vulkan surface has no supported formats");
	}

	Vector<VkSurfaceFormatKHR> formats(formatCount);
	VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data()));
	VkSurfaceFormatKHR surfaceFormat = formats[0];
	for (const VkSurfaceFormatKHR& format : formats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			surfaceFormat = format;
			break;
		}
	}


	U32 imageCount = std::max(capabilities.minImageCount, m_swapchainDesc.frameCount);
	if (capabilities.maxImageCount) imageCount = std::min(imageCount, capabilities.maxImageCount);
	VkSwapchainCreateInfoKHR createInfo{
		.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface          = m_surface,
		.minImageCount    = imageCount,
		.imageFormat      = surfaceFormat.format,
		.imageColorSpace  = surfaceFormat.colorSpace,
		.imageExtent      = extent,
		.imageArrayLayers = 1,
		.imageUsage       = ToVkImageUsage(m_swapchainDesc.imageUsage),
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform     = capabilities.currentTransform,
		.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode      = m_swapchainDesc.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR,
		.clipped          = VK_TRUE,
	};

	VK_TRY(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain));
	VK_TRY(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr));

	Vector<VkImage> images(imageCount);
	VK_TRY(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, images.data()));

	Format swapchainFormat = FromVkFormat(surfaceFormat.format);
	if (swapchainFormat == Format::None) return Err("Unsupported swapchain format: %u", (U32)surfaceFormat.format);
	for (VkImage vkImage : images) {
		Image* image = CreateImage({
			.name             = "swapchain image",
			.width            = extent.width,
			.height           = extent.height,
			.format           = swapchainFormat,
			.usage            = m_swapchainDesc.imageUsage,
			.ownedBySwapchain = true,
			.existingImage    = vkImage,
		});
		if (!image) return Err("Failed to create swapchain image view");
		m_swapchainImages.push_back(image);
	}

	m_renderDoneSemaphores.resize(m_swapchainImages.size());
	for (VkSemaphore& semaphore : m_renderDoneSemaphores)
		TRY(CreateSemaphore(&semaphore));
	TRY(CreateSemaphore(&m_imageAcquiredSemaphore));

	m_needsNewSwapchain = false;
	return Success();
}

Result GraphicsDevice_Vulkan::CreateSemaphore(VkSemaphore* outSemaphore) {
	VkSemaphoreCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};
	VK_TRY(vkCreateSemaphore(m_device, &createInfo, nullptr, outSemaphore));
	return Success();
}

Result GraphicsDevice_Vulkan::Init(const GraphicsDeviceInitDesc& desc) {
	if (volkInitialize() != VK_SUCCESS) {
		return Err("volkInitialize failed");
	}

	{ // create instance:
		U32 instanceExtensionCount = 0;
		const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&instanceExtensionCount);
		if (!sdlExtensions) return Err("SDL_Vulkan_GetInstanceExtensions: %s", SDL_GetError());
		Vector<const char*> instance_extensions(sdlExtensions, sdlExtensions + instanceExtensionCount);

		const char* validationLayer = "VK_LAYER_KHRONOS_validation";
		Vector<const char*> layers;
		if (desc.enableDebug && HasInstanceLayer(validationLayer)) layers.push_back(validationLayer);

		VkApplicationInfo applicationInfo{
			.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName   = "EVA",
			.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
			.pEngineName        = "EVA",
			.engineVersion      = VK_MAKE_API_VERSION(0, 0, 1, 0),
			.apiVersion         = VK_API_VERSION_1_3,
		};
		VkInstanceCreateInfo createInfo{
			.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo        = &applicationInfo,
			.enabledLayerCount       = (U32)layers.size(),
			.ppEnabledLayerNames     = layers.data(),
			.enabledExtensionCount   = (U32)instance_extensions.size(),
			.ppEnabledExtensionNames = instance_extensions.data(),
		};

		VK_TRY(vkCreateInstance(&createInfo, nullptr, &m_instance));
		volkLoadInstance(m_instance);
	}

	{ // create surface:
		if (!SDL_Vulkan_CreateSurface(desc.window, m_instance, nullptr, &m_surface))
			return Err("SDL_Vulkan_CreateSurface: %s", SDL_GetError());
	}

	{ // pick physical device:
		U32 physicalDeviceCount = 0;
		VK_TRY(vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr));
		if (physicalDeviceCount == 0) return Err("No Vulkan physical devices found");

		Vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		VK_TRY(vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data()));
		I64 bestScore = -1;
		for (VkPhysicalDevice physicalDevice : physicalDevices) {
			U32 graphicsFamily = UINT32_MAX;
			I64 score = ScorePhysicalDevice(physicalDevice, &graphicsFamily);
			if (score > bestScore) {
				bestScore = score;
				m_physicalDevice = physicalDevice;
				m_graphicsFamily = graphicsFamily;
			}
		}
		if (!m_physicalDevice) return Err("No Vulkan 1.3 physical device with a graphics queue found");
	}

	{ // create device:
		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = m_graphicsFamily,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority,
		};

		VkPhysicalDeviceVulkan12Features features12{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
			.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
			.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
			.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
			.descriptorBindingPartiallyBound = VK_TRUE,
			.runtimeDescriptorArray = VK_TRUE,
			.scalarBlockLayout = VK_TRUE,
		};
		VkPhysicalDeviceVulkan13Features features13{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &features12,
			.synchronization2 = VK_TRUE,
			.dynamicRendering = VK_TRUE,
		};
		const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		VkPhysicalDeviceFeatures features{
			.samplerAnisotropy = VK_TRUE,
		};
		VkDeviceCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &features13,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queueCreateInfo,
			.enabledExtensionCount = 1,
			.ppEnabledExtensionNames = deviceExtensions,
			.pEnabledFeatures = &features,
		};
		VK_TRY(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));
		volkLoadDevice(m_device);
		vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
	}

	{ // set up descriptors:
		VkPhysicalDeviceDescriptorIndexingProperties descriptorIndexingProperties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES,
		};
		VkPhysicalDeviceProperties2 properties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &descriptorIndexingProperties,
		};
		vkGetPhysicalDeviceProperties2(m_physicalDevice, &properties);

		if (!m_bindlessBufferCount || !m_bindlessImageCount || !m_bindlessSamplerCount)
			return Err("Vulkan device has insufficient update-after-bind descriptor limits");

		VkDescriptorSetLayoutBinding bindings[] = {
			{
				.binding         = 0,
				.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = m_bindlessBufferCount,
				.stageFlags      = VK_SHADER_STAGE_ALL,
			},
			{
				.binding         = 1,
				.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = m_bindlessImageCount,
				.stageFlags      = VK_SHADER_STAGE_ALL,
			},
			{
				.binding         = 2,
				.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = m_bindlessSamplerCount,
				.stageFlags      = VK_SHADER_STAGE_ALL,
			},
		};
		VkDescriptorBindingFlags bindingFlags[] = {
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		};
		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount  = EVA_ARRAYSIZE(bindingFlags),
			.pBindingFlags = bindingFlags,
		};
		VkDescriptorSetLayoutCreateInfo createInfo{
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext        = &bindingFlagsInfo,
			.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
			.bindingCount = EVA_ARRAYSIZE(bindings),
			.pBindings    = bindings,
		};
		VK_TRY(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_bindlessDescriptorSetLayout));

		VkDescriptorPoolSize poolSizes[] = {
			{ .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = m_bindlessBufferCount,  },
			{ .type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  .descriptorCount = m_bindlessImageCount,   },
			{ .type            = VK_DESCRIPTOR_TYPE_SAMPLER,        .descriptorCount = m_bindlessSamplerCount, },
		};
		VkDescriptorPoolCreateInfo poolCreateInfo{
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
			.maxSets       = 1,
			.poolSizeCount = EVA_ARRAYSIZE(poolSizes),
			.pPoolSizes    = poolSizes,
		};
		VK_TRY(vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &m_bindlessDescriptorPool));

		VkDescriptorSetAllocateInfo allocateInfo{
			.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool     = m_bindlessDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts        = &m_bindlessDescriptorSetLayout,
		};
		VK_TRY(vkAllocateDescriptorSets(m_device, &allocateInfo, &m_bindlessDescriptorSet));

		VkPushConstantRange pushConstantRange{
			.stageFlags = VK_SHADER_STAGE_ALL,
			.offset     = 0,
			.size       = properties.properties.limits.maxPushConstantsSize,
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount         = 1,
			.pSetLayouts            = &m_bindlessDescriptorSetLayout,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges    = &pushConstantRange,
		};
		VK_TRY(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));
		m_bindlessBuffers.Init(m_bindlessBufferCount);
		m_bindlessImages.Init(m_bindlessImageCount);
		m_bindlessSamplers.Init(m_bindlessSamplerCount);
	}

	{ // create allocator:
		VmaVulkanFunctions functions{
			.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
			.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		};
		VmaAllocatorCreateInfo createInfo{
			.physicalDevice = m_physicalDevice,
			.device = m_device,
			.pVulkanFunctions = &functions,
			.instance = m_instance,
			.vulkanApiVersion = VK_API_VERSION_1_3,
		};
		VK_TRY(vmaCreateAllocator(&createInfo, &m_allocator));
	}

	{ // create a swapchain:
		TRY(CreateSwapchain());
		TRY(CreateFence(true, &m_frameFence));
		if (!m_frameFence) return Err("Failed to create frame fence");
	}

	{ // create frame command buffers:
		TRY(CreateCommandPool(&m_mainCommandPool));
		TRY(CreateCommandPool(&m_transferCommandPool));
		TRY(CreateCommandBuffer(m_mainCommandPool, &m_mainCommandBuffer));
		TRY(CreateCommandBuffer(m_transferCommandPool, &m_transferCommandBuffer));
		TRY(BeginFrameCommandBuffers());
	}

	return Success();
}

GraphicsDevice_Vulkan::~GraphicsDevice_Vulkan() {
	if (m_device) vkDeviceWaitIdle(m_device);
	if (m_frameFence) vkDestroyFence(m_device, m_frameFence, nullptr);
	if (m_imageAcquiredSemaphore) vkDestroySemaphore(m_device, m_imageAcquiredSemaphore, nullptr);
	DestroySwapchain();
	delete m_mainCommandBuffer;
	delete m_transferCommandBuffer;
	if (m_mainCommandPool) vkDestroyCommandPool(m_device, m_mainCommandPool, nullptr);
	if (m_transferCommandPool) vkDestroyCommandPool(m_device, m_transferCommandPool, nullptr);
	if (m_allocator) vmaDestroyAllocator(m_allocator);
	if (m_pipelineLayout) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	if (m_bindlessDescriptorPool) vkDestroyDescriptorPool(m_device, m_bindlessDescriptorPool, nullptr);
	if (m_bindlessDescriptorSetLayout) vkDestroyDescriptorSetLayout(m_device, m_bindlessDescriptorSetLayout, nullptr);
	if (m_device) vkDestroyDevice(m_device, nullptr);
	if (m_surface) SDL_Vulkan_DestroySurface(m_instance, m_surface, nullptr);
	if (m_instance) vkDestroyInstance(m_instance, nullptr);
}

void CommandBuffer_Vulkan::BeginRendering(const RenderingDesc& desc) {
	assert(desc.colorAttachmentCount <= 4);
	assert(desc.colorAttachmentCount || desc.depthAttachment);

	Image* extentImage = desc.colorAttachmentCount ? desc.colorAttachments[0].image : desc.depthAttachment->image;
	assert(extentImage);

	VkRenderingAttachmentInfo colorAttachments[4] = {};
	for (U32 i = 0; i < desc.colorAttachmentCount; i++) {
		const AttachmentDesc& attachment = desc.colorAttachments[i];
		assert(attachment.image);
		ImageBarrier({
			.image       = attachment.image,
			.stateBefore = attachment.stateBefore,
			.stateAfter  = attachment.stateDuring,
		});
		colorAttachments[i] = VkRenderingAttachmentInfo{
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView   = attachment.image->m_vulkan.imageView,
			.imageLayout = ImageStateToImageLayout(attachment.stateDuring),
			.loadOp      = ToVkLoadOp(attachment.loadOp),
			.storeOp     = ToVkStoreOp(attachment.storeOp),
			.clearValue  = {
				.color = {{ attachment.clearColor.x, attachment.clearColor.y, attachment.clearColor.z, attachment.clearColor.w }},
			},
		};
	}

	VkRenderingAttachmentInfo depthAttachment{};
	if (desc.depthAttachment) {
		const AttachmentDesc& attachment = *desc.depthAttachment;
		assert(attachment.image);
		ImageBarrier({
			.image       = attachment.image,
			.stateBefore = attachment.stateBefore,
			.stateAfter  = attachment.stateDuring,
		});
		depthAttachment = VkRenderingAttachmentInfo{
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView   = attachment.image->m_vulkan.imageView,
			.imageLayout = ImageStateToImageLayout(attachment.stateDuring),
			.loadOp      = ToVkLoadOp(attachment.loadOp),
			.storeOp     = ToVkStoreOp(attachment.storeOp),
			.clearValue  = { .depthStencil = { attachment.clearDepth, attachment.clearStencil } },
		};
	}

	VkRenderingInfo renderingInfo{
		.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea           = {{0, 0}, {extentImage->m_width, extentImage->m_height}},
		.layerCount           = 1,
		.colorAttachmentCount = desc.colorAttachmentCount,
		.pColorAttachments    = colorAttachments,
		.pDepthAttachment     = desc.depthAttachment ? &depthAttachment : nullptr,
		.pStencilAttachment   = desc.depthAttachment && FormatHasStencil(desc.depthAttachment->image->m_format) ? &depthAttachment : nullptr,
	};
	vkCmdBeginRendering(m_vk, &renderingInfo);

	VkViewport viewport{ 0.0f, (float)extentImage->m_height, (float)extentImage->m_width, -(float)extentImage->m_height, 0.0f, 1.0f, };
	VkRect2D scissor{ {0, 0}, {extentImage->m_width, extentImage->m_height} };
	vkCmdSetViewport(m_vk, 0, 1, &viewport);
	vkCmdSetScissor(m_vk, 0, 1, &scissor);
}

void CommandBuffer_Vulkan::EndRendering(const RenderingDesc& desc) {
	vkCmdEndRendering(m_vk);

	for (U32 i = 0; i < desc.colorAttachmentCount; i++) {
		const AttachmentDesc& attachment = desc.colorAttachments[i];
		assert(attachment.image);
		if (attachment.stateDuring != attachment.stateAfter) {
			ImageBarrier({
				.image       = attachment.image,
				.stateBefore = attachment.stateDuring,
				.stateAfter  = attachment.stateAfter,
			});
		}
	}

	if (desc.depthAttachment) {
		const AttachmentDesc& attachment = *desc.depthAttachment;
		assert(attachment.image);
		if (attachment.stateDuring != attachment.stateAfter) {
			ImageBarrier({
				.image       = attachment.image,
				.stateBefore = attachment.stateDuring,
				.stateAfter  = attachment.stateAfter,
			});
		}
	}
}

void CommandBuffer_Vulkan::BindPipeline(GraphicsPipeline* pipeline) {
	vkCmdBindPipeline(m_vk, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->m_vulkan.pipeline);
}

void CommandBuffer_Vulkan::BindIndexBuffer(GPUBuffer* buffer, IndexType type, U64 offset) {
	VkIndexType indexType = type == IndexType::U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	vkCmdBindIndexBuffer(m_vk, buffer->m_vk, offset, indexType);
}

void CommandBuffer_Vulkan::PushConstants(GraphicsPipeline* pipeline, U32 size, const void* data) {
	vkCmdPushConstants(m_vk, pipeline->m_vulkan.layout, VK_SHADER_STAGE_ALL, 0, size, data);
}

void CommandBuffer_Vulkan::Draw(U32 vertexCount, U32 instanceCount, U32 firstVertex, U32 firstInstance) {
	vkCmdDraw(m_vk, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer_Vulkan::DrawIndexed(U32 indexCount, U32 instanceCount, U32 firstIndex, I32 vertexOffset, U32 firstInstance) {
	vkCmdDrawIndexed(m_vk, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer_Vulkan::CopyBuffer(GPUBuffer* source, GPUBuffer* destination, const BufferCopyDesc& copy) {
	VkBufferCopy region{
		.srcOffset = copy.sourceOffset,
		.dstOffset = copy.destOffset,
		.size = copy.size,
	};
	vkCmdCopyBuffer(m_vk, source->m_vk, destination->m_vk, 1, &region);
}

void CommandBuffer_Vulkan::CopyBufferToImage(GPUBuffer* source, Image* destination, const BufferImageCopyDesc& copy) {
	VkBufferImageCopy region{
		.bufferOffset = copy.bufferOffset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask = (VkImageAspectFlags)(FormatIsDepth(destination->m_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
			.mipLevel = copy.imageMip,
			.baseArrayLayer = copy.imageLayer,
			.layerCount = 1,
		},
		.imageOffset = { (I32)copy.imageX, (I32)copy.imageY, (I32)copy.imageZ },
		.imageExtent = { copy.width, copy.height, copy.depth },
	};
	vkCmdCopyBufferToImage(m_vk, source->m_vk, destination->m_vulkan.image,
		ImageStateToImageLayout(destination->m_state), 1, &region);
}

void CommandBuffer_Vulkan::ImageBarrier(const ImageBarrierDesc& barrier) {
	Image* image = barrier.image;
	VkImageAspectFlags aspect = FormatIsDepth(image->m_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	if (FormatHasStencil(image->m_format)) aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	VkImageMemoryBarrier2 imageBarrier{
		.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask        = ImageStateToPipelineStage(barrier.stateBefore),
		.srcAccessMask       = ImageStateToAccessFlags(barrier.stateBefore),
		.dstStageMask        = ImageStateToPipelineStage(barrier.stateAfter),
		.dstAccessMask       = ImageStateToAccessFlags(barrier.stateAfter),
		.oldLayout           = ImageStateToImageLayout(barrier.stateBefore),
		.newLayout           = ImageStateToImageLayout(barrier.stateAfter),
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = image->m_vulkan.image,
		.subresourceRange = {
			.aspectMask = aspect,
			.baseMipLevel = barrier.baseMip,
			.levelCount = barrier.mipCount,
			.baseArrayLayer = barrier.baseLayer,
			.layerCount = barrier.layerCount,
		},
	};
	VkDependencyInfo dependencyInfo{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &imageBarrier,
	};
	vkCmdPipelineBarrier2(m_vk, &dependencyInfo);
	image->m_state = barrier.stateAfter;
}

void CommandBuffer_Vulkan::GenerateMipmaps(Image*) {
	printf("STUB: GenerateMipmaps\n");
}

bool GraphicsDevice_Vulkan::BeginFrame() {
	if (m_needsNewSwapchain) {
		WaitIdle();
		DestroySwapchain();
		CreateSwapchain();
	}
	if (m_needsNewSwapchain) {
		return false;
	}

	vkWaitForFences(m_device, 1, &m_frameFence, VK_TRUE, UINT64_MAX);
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAcquiredSemaphore, VK_NULL_HANDLE, &m_swapchainImageIndex);

	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		m_needsNewSwapchain = true;
		return false;
	}
	BeginFrameCommandBuffers();
	return true;
}

void GraphicsDevice_Vulkan::EndFrame() {
	VkSemaphore renderDoneSemaphore = m_renderDoneSemaphores[m_swapchainImageIndex];

	VkMemoryBarrier2 transferBarrier{
		.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
		.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
	};
	VkDependencyInfo dependencyInfo{
		.sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers    = &transferBarrier,
	};
	vkCmdPipelineBarrier2(m_transferCommandBuffer->m_vk, &dependencyInfo);
	vkEndCommandBuffer(m_transferCommandBuffer->m_vk);
	vkEndCommandBuffer(m_mainCommandBuffer->m_vk);

	VkCommandBufferSubmitInfo commandBuffers[] = {
		{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = m_transferCommandBuffer->m_vk, },
		{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = m_mainCommandBuffer->m_vk, },
	};
	VkSemaphoreSubmitInfo imageAcquiredWait{
		.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = m_imageAcquiredSemaphore,
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	};
	VkSemaphoreSubmitInfo renderDoneSignal{
		.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = renderDoneSemaphore,
		.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
	};
	VkSubmitInfo2 mainSubmit{
		.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount   = 1,
		.pWaitSemaphoreInfos      = &imageAcquiredWait,
		.commandBufferInfoCount   = 2,
		.pCommandBufferInfos      = commandBuffers,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos    = &renderDoneSignal,
	};
	vkResetFences(m_device, 1, &m_frameFence);
	vkQueueSubmit2(m_graphicsQueue, 1, &mainSubmit, m_frameFence);

	VkPresentInfoKHR presentInfo{
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores    = &renderDoneSemaphore,
		.swapchainCount     = 1,
		.pSwapchains        = &m_swapchain,
		.pImageIndices      = &m_swapchainImageIndex,
	};
	vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
	m_frameCommandBuffersBegun = false;
}

CommandBuffer* GraphicsDevice_Vulkan::GetMainCommandBuffer() {
	return m_mainCommandBuffer;
}

CommandBuffer* GraphicsDevice_Vulkan::GetTransferCommandBuffer() {
	return m_transferCommandBuffer;
}

Image* GraphicsDevice_Vulkan::GetCurrentBackbuffer() {
	return m_swapchainImages.empty() ? nullptr : m_swapchainImages[m_swapchainImageIndex];
}

void GraphicsDevice_Vulkan::WaitIdle() {
	vkDeviceWaitIdle(m_device);
}

Result GraphicsDevice_Vulkan::CreateFence(bool signaled, VkFence* out_fence) {
	VkFenceCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = signaled ? (VkFenceCreateFlags)VK_FENCE_CREATE_SIGNALED_BIT : 0,
	};
	VK_TRY(vkCreateFence(m_device, &createInfo, nullptr, out_fence));
	return Success();
}

GPUBuffer* GraphicsDevice_Vulkan::CreateGPUBuffer(const GPUBufferDesc& desc) {
	ScratchArena scratch;

	VkBufferUsageFlags usage = 0;
	if (desc.usage & GPUBufferUsage_TransferSource) usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (desc.usage & GPUBufferUsage_TransferDest) usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (desc.usage & GPUBufferUsage_VertexBuffer) usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (desc.usage & GPUBufferUsage_IndexBuffer) usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (desc.usage & GPUBufferUsage_ConstantBuffer) usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (desc.usage & GPUBufferUsage_StorageBuffer) usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (desc.usage & GPUBufferUsage_IndirectBuffer) usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	VmaAllocationCreateInfo allocationCreateInfo{};
	switch (desc.memoryUsage) {
		case MemoryUsage::GPUOnly:
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			break;
		case MemoryUsage::CPUToGPU:
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
		case MemoryUsage::GPUToCPU:
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
	}

	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = desc.size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	GPUBuffer* buffer = new GPUBuffer();
	VmaAllocationInfo allocationInfo{};
	VkResult result = vmaCreateBuffer(m_allocator, &bufferCreateInfo, &allocationCreateInfo, &buffer->m_vk, &buffer->m_vmaAllocation, &allocationInfo);
	if (result != VK_SUCCESS) {
		delete buffer;
		return nullptr;
	}
	if (desc.name.size)
		vmaSetAllocationName(m_allocator, buffer->m_vmaAllocation, desc.name.CopyToArena(scratch));

	buffer->m_size = desc.size;
	buffer->m_mapped = allocationInfo.pMappedData;
	if (desc.bindless) {
		buffer->m_bindlessIndex = m_bindlessBuffers.Register(buffer);
		VkDescriptorBufferInfo bufferInfo{
			.buffer = buffer->m_vk,
			.offset = 0,
			.range  = desc.size,
		};
		VkWriteDescriptorSet write{
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = m_bindlessDescriptorSet,
			.dstBinding      = 0,
			.dstArrayElement = buffer->m_bindlessIndex,
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo     = &bufferInfo,
		};
		vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
	}
	return buffer;
}

void GraphicsDevice_Vulkan::DestroyGPUBuffer(GPUBuffer* buffer) {
	if (!buffer) return;
	if (buffer->m_bindlessIndex != UINT32_MAX) {
		m_bindlessBuffers.Free(buffer->m_bindlessIndex);
		buffer->m_bindlessIndex = UINT32_MAX;
	}
	vmaDestroyBuffer(m_allocator, buffer->m_vk, buffer->m_vmaAllocation);
	delete buffer;
}

Image* GraphicsDevice_Vulkan::CreateImage(const ImageDesc& desc) {
	ScratchArena scratch;

	VkImageCreateInfo imageCreateInfo{
		.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType     = VK_IMAGE_TYPE_2D,
		.format        = ToVkFormat(desc.format),
		.extent        = { desc.width, desc.height, 1 },
		.mipLevels     = desc.mipCount,
		.arrayLayers   = 1,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.tiling        = VK_IMAGE_TILING_OPTIMAL,
		.usage         = ToVkImageUsage(desc.usage),
		.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
	};
	VmaAllocationCreateInfo allocationCreateInfo{};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkImage vkImage = {};
	VmaAllocation vmaAllocation = {};

	VkResult result = VK_SUCCESS;
	if (desc.existingImage) {
		vkImage = desc.existingImage;
	} else {
		VkResult res = vmaCreateImage(m_allocator, &imageCreateInfo, &allocationCreateInfo, &vkImage, &vmaAllocation, nullptr);
		if (res != VK_SUCCESS) {
			return nullptr;
		}
		if (desc.name.size)
			vmaSetAllocationName(m_allocator, vmaAllocation, desc.name.CopyToArena(scratch));
	}

	Image* image = new Image();
	image->m_vulkan.image = vkImage;
	image->m_vulkan.allocation = vmaAllocation;

	VkImageAspectFlags aspect = FormatIsDepth(desc.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	if (FormatHasStencil(desc.format)) aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VkImageViewCreateInfo viewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image->m_vulkan.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = imageCreateInfo.format,
		.subresourceRange = { aspect, 0, desc.mipCount, 0, 1 },
	};
	result = vkCreateImageView(m_device, &viewCreateInfo, nullptr, &image->m_vulkan.imageView);
	if (result != VK_SUCCESS) {
		if (!desc.ownedBySwapchain)
			vmaDestroyImage(m_allocator, image->m_vulkan.image, image->m_vulkan.allocation);
		delete image;
		return nullptr;
	}

	image->m_width = desc.width;
	image->m_height = desc.height;
	image->m_mipCount = desc.mipCount;
	image->m_format = desc.format;
	image->m_state = ImageState::Undefined;
	image->m_ownedBySwapchain = desc.ownedBySwapchain;

	if (desc.bindless) {
		image->m_bindlessIndex = m_bindlessImages.Register(image);
		VkDescriptorImageInfo imageInfo{
			.imageView   = image->m_vulkan.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		VkWriteDescriptorSet write{
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = m_bindlessDescriptorSet,
			.dstBinding      = 1,
			.dstArrayElement = image->m_bindlessIndex,
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo      = &imageInfo,
		};
		vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
	}
	return image;
}

void GraphicsDevice_Vulkan::DestroyImage(Image* image) {
	if (!image) return;
	if (image->m_bindlessIndex != UINT32_MAX) {
		m_bindlessImages.Free(image->m_bindlessIndex);
		image->m_bindlessIndex = UINT32_MAX;
	}
	if (image->m_vulkan.imageView) vkDestroyImageView(m_device, image->m_vulkan.imageView, nullptr);
	if (image->m_vulkan.image && !image->m_ownedBySwapchain)
		vmaDestroyImage(m_allocator, image->m_vulkan.image, image->m_vulkan.allocation);
	delete image;
}

Sampler* GraphicsDevice_Vulkan::CreateSampler(const SamplerDesc& desc) {
	VkSamplerCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = ToVkFilter(desc.magFilter),
		.minFilter = ToVkFilter(desc.minFilter),
		.mipmapMode = ToVkMipmapMode(desc.mipmapMode),
		.addressModeU = ToVkAddressMode(desc.addressU),
		.addressModeV = ToVkAddressMode(desc.addressV),
		.addressModeW = ToVkAddressMode(desc.addressW),
		.mipLodBias = desc.mipLodBias,
		.anisotropyEnable = desc.anisotropyEnable,
		.maxAnisotropy = desc.maxAnisotropy,
		.compareEnable = desc.compareEnable,
		.compareOp = ToVkCompareOp(desc.compareOp),
		.minLod = desc.minLod,
		.maxLod = desc.maxLod,
		.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};
	Sampler* sampler = new Sampler();
	if (vkCreateSampler(m_device, &createInfo, nullptr, &sampler->m_vk) != VK_SUCCESS) {
		delete sampler;
		return nullptr;
	}
	if (desc.bindless) {
		sampler->m_bindlessIndex = m_bindlessSamplers.Register(sampler, desc.forcedBindlessIndex);
		VkDescriptorImageInfo imageInfo{
			.sampler = sampler->m_vk,
		};
		VkWriteDescriptorSet write{
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = m_bindlessDescriptorSet,
			.dstBinding      = 2,
			.dstArrayElement = sampler->m_bindlessIndex,
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
			.pImageInfo      = &imageInfo,
		};
		vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
	}
	return sampler;
}

void GraphicsDevice_Vulkan::DestroySampler(Sampler* sampler) {
	if (!sampler) return;
	if (sampler->m_bindlessIndex != UINT32_MAX) {
		m_bindlessSamplers.Free(sampler->m_bindlessIndex);
		sampler->m_bindlessIndex = UINT32_MAX;
	}
	if (sampler->m_vk) vkDestroySampler(m_device, sampler->m_vk, nullptr);
	delete sampler;
}

Sampler* GraphicsDevice_Vulkan::GetSampler(U32 index) {
	return m_bindlessSamplers.Get(index);
}

ShaderModule* GraphicsDevice_Vulkan::CreateShaderModule(const ShaderModuleDesc& desc) {
	if (!desc.code || !desc.codeSize || desc.codeSize % sizeof(U32) != 0) return nullptr;

	ShaderModule* shader = new ShaderModule();
	VkShaderModuleCreateInfo createInfo{
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = desc.codeSize,
		.pCode    = desc.code,
	};
	if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shader->m_vk) != VK_SUCCESS) {
		delete shader;
		return nullptr;
	}
	return shader;
}

void GraphicsDevice_Vulkan::DestroyShaderModule(ShaderModule* shader) {
	if (!shader) return;
	if (shader->m_vk) vkDestroyShaderModule(m_device, shader->m_vk, nullptr);
	delete shader;
}

GraphicsPipeline* GraphicsDevice_Vulkan::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) {
	if (!desc.vertexShader || !desc.vertexShader->m_vk || !desc.fragmentShader || !desc.fragmentShader->m_vk)
		return nullptr;

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
	if (desc.pushConstantSize > deviceProperties.limits.maxPushConstantsSize) return nullptr;

	VkPipelineShaderStageCreateInfo shaderStages[] = {
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = desc.vertexShader->m_vk,
			.pName  = "main",
		},
		{
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = desc.fragmentShader->m_vk,
			.pName  = "main",
		},
	};
	VkPipelineVertexInputStateCreateInfo vertexInputState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	};
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology               = ToVkPrimitiveTopology(desc.topology),
		.primitiveRestartEnable = VK_FALSE,
	};
	VkPipelineTessellationStateCreateInfo tessellationState{
		.sType              = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.patchControlPoints = 3,
	};
	VkPipelineViewportStateCreateInfo viewportState{
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount  = 1,
	};
	VkPipelineRasterizationStateCreateInfo rasterizationState{
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable        = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode             = ToVkPolygonMode(desc.polygonMode),
		.cullMode                = ToVkCullMode(desc.cullMode),
		.frontFace               = ToVkFrontFace(desc.frontFace),
		.depthBiasEnable         = VK_FALSE,
		.lineWidth               = 1.0f,
	};
	VkPipelineMultisampleStateCreateInfo multisampleState{
		.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable   = VK_FALSE,
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilState{
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable       = desc.depthTestEnable,
		.depthWriteEnable      = desc.depthWriteEnable,
		.depthCompareOp        = ToVkCompareOp(desc.depthCompare),
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable     = VK_FALSE,
		.minDepthBounds        = 0.0f,
		.maxDepthBounds        = 1.0f,
	};

	VkFormat colorFormats[4] = {};
	VkPipelineColorBlendAttachmentState colorBlendAttachments[4] = {};
	BlendState blendState = BlendModeToBlendState(desc.blendMode);
	U32 colorAttachmentCount = 0;
	for (U32 i = 0; i < EVA_ARRAYSIZE(desc.format.colorFormat); i++) {
		if (desc.format.colorFormat[i] == Format::None) {
			for (U32 j = i + 1; j < EVA_ARRAYSIZE(desc.format.colorFormat); j++)
				if (desc.format.colorFormat[j] != Format::None) return nullptr;
			break;
		}
		colorFormats[colorAttachmentCount] = ToVkFormat(desc.format.colorFormat[i]);
		if (colorFormats[colorAttachmentCount] == VK_FORMAT_UNDEFINED) return nullptr;
		colorBlendAttachments[colorAttachmentCount] = VkPipelineColorBlendAttachmentState{
			.blendEnable         = blendState.blendEnable,
			.srcColorBlendFactor = ToVkBlendFactor(blendState.sourceColorBlend),
			.dstColorBlendFactor = ToVkBlendFactor(blendState.destColorBlend),
			.colorBlendOp        = ToVkBlendOp(blendState.colorBlendOp),
			.srcAlphaBlendFactor = ToVkBlendFactor(blendState.sourceAlphaBlend),
			.dstAlphaBlendFactor = ToVkBlendFactor(blendState.destAlphaBlend),
			.alphaBlendOp        = ToVkBlendOp(blendState.alphaBlendOp),
			.colorWriteMask      = 0x0F,
		};
		colorAttachmentCount++;
	}
	VkPipelineColorBlendStateCreateInfo colorBlendState{
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable   = VK_FALSE,
		.attachmentCount = colorAttachmentCount,
		.pAttachments    = colorBlendAttachments,
	};
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamicState{
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = EVA_ARRAYSIZE(dynamicStates),
		.pDynamicStates    = dynamicStates,
	};

	GraphicsPipeline* pipeline = new GraphicsPipeline();
	pipeline->m_vulkan.layout = m_pipelineLayout;

	VkFormat depthFormat = ToVkFormat(desc.format.depthFormat);
	VkPipelineRenderingCreateInfo renderingInfo{
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount    = colorAttachmentCount,
		.pColorAttachmentFormats = colorFormats,
		.depthAttachmentFormat   = depthFormat,
		.stencilAttachmentFormat = FormatHasStencil(desc.format.depthFormat) ? depthFormat : VK_FORMAT_UNDEFINED,
	};
	VkGraphicsPipelineCreateInfo createInfo{
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext               = &renderingInfo,
		.stageCount          = EVA_ARRAYSIZE(shaderStages),
		.pStages             = shaderStages,
		.pVertexInputState   = &vertexInputState,
		.pInputAssemblyState = &inputAssemblyState,
		.pTessellationState  = desc.topology == PrimitiveTopology::PatchList ? &tessellationState : nullptr,
		.pViewportState      = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pMultisampleState   = &multisampleState,
		.pDepthStencilState  = &depthStencilState,
		.pColorBlendState    = &colorBlendState,
		.pDynamicState       = &dynamicState,
		.layout              = pipeline->m_vulkan.layout,
		.renderPass          = VK_NULL_HANDLE,
		.subpass             = 0,
	};
	if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->m_vulkan.pipeline) != VK_SUCCESS) {
		delete pipeline;
		return nullptr;
	}
	return pipeline;
}

void GraphicsDevice_Vulkan::DestroyGraphicsPipeline(GraphicsPipeline* pipeline) {
	if (!pipeline) return;
	if (pipeline->m_vulkan.pipeline) vkDestroyPipeline(m_device, pipeline->m_vulkan.pipeline, nullptr);
	delete pipeline;
}

SwapchainDesc GraphicsDevice_Vulkan::GetSwapchainDesc() {
	return m_swapchainDesc;
}

void GraphicsDevice_Vulkan::SetSwapchainDesc(SwapchainDesc desc) {
	m_swapchainDesc = desc;
	m_needsNewSwapchain = true;
}

CommandBuffer* GraphicsDevice_Vulkan::BeginImmediateCommandBuffer() {
	assert(0);
	return {};
	// CommandBuffer* cmd = CreateCommandBuffer();
	// BeginCommandBuffer(cmd);
	// return cmd;
}

void GraphicsDevice_Vulkan::FlushImmediateCommandBuffer(CommandBuffer* cmd) {
	assert(0);
	// EndCommandBuffer(cmd);
	// VkFence fence;
	// CreateFence(false, &fence);

	// VkCommandBufferSubmitInfo commandBufferSubmitInfo[] = {
	// 	{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = cmd->m_vk, },
	// };
	// VkSubmitInfo2 mainSubmit{
	// 	.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
	// 	.commandBufferInfoCount   = EVA_ARRAYSIZE(commandBufferSubmitInfo),
	// 	.pCommandBufferInfos      = commandBufferSubmitInfo,
	// };
	// vkQueueSubmit2(m_graphicsQueue, 1, &mainSubmit, m_frameFence);
	// vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

	// vkDestroyFence(m_device, fence, nullptr);
	
	// DestroyCommandBuffer(cmd);
}
