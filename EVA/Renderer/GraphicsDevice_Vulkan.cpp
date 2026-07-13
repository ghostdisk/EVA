#include <volk.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include <Vendor/vk_mem_alloc.h>
#include <EVA/Renderer/GraphicsDevice_Vulkan.hpp>
#include <EVA/Core/Common.hpp>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <algorithm>

#define VK_TRY(expr) \
	do { \
		VkResult res = (expr); \
		if (res != VK_SUCCESS) { \
			return Err("Vulkan Error %u at %s:%d", (U32)res, __FILE__, __LINE__); \
		} \
	} while (0)


namespace GFX {

static bool HasInstanceLayer(String name) {
	U32 count = 0;
	vkEnumerateInstanceLayerProperties(&count, nullptr);
	std::vector<VkLayerProperties> layers(count);
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

static bool IsDepthFormat(Format format) {
	return format >= Format::D16_UNORM;
}

static bool HasStencil(Format format) {
	return format == Format::D16_UNORM_S8_UINT || format == Format::D24_UNORM_S8_UINT || format == Format::D32_FLOAT_S8_UINT;
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

static I64 ScorePhysicalDevice(VkPhysicalDevice physicalDevice, U32* outGraphicsFamily) {
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	if (properties.apiVersion < VK_API_VERSION_1_3) return -1;

	U32 queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
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
	CommandBuffer_Vulkan* commandBuffer = new CommandBuffer_Vulkan();
	VkCommandBufferAllocateInfo allocateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkResult result = vkAllocateCommandBuffers(m_device, &allocateInfo, &commandBuffer->m_vk);
	if (result != VK_SUCCESS) {
		delete commandBuffer;
		return Err("Vulkan Error %u at %s:%d", (U32)result, __FILE__, __LINE__);
	}
	*outCommandBuffer = commandBuffer;
	return Success();
}

Result GraphicsDevice_Vulkan::BeginFrameCommandBuffers() {
	if (m_frameCommandBuffersBegun) return Success();

	VK_TRY(vkResetCommandPool(m_device, m_mainCommandPool, 0));
	VK_TRY(vkResetCommandPool(m_device, m_transferCommandPool, 0));
	VkCommandBufferBeginInfo beginInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_TRY(vkBeginCommandBuffer(m_mainCommandBuffer->m_vk, &beginInfo));
	VK_TRY(vkBeginCommandBuffer(m_transferCommandBuffer->m_vk, &beginInfo));
	m_frameCommandBuffersBegun = true;
	return Success();
}

Result GraphicsDevice_Vulkan::CreateSwapchain(const GraphicsDeviceInitDesc& desc) {
	VkSurfaceCapabilitiesKHR capabilities;
	VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities));

	U32 formatCount = 0;
	VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr));
	if (!formatCount) {
		return Err("Vulkan surface has no supported formats");
	}

	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data()));
	VkSurfaceFormatKHR surfaceFormat = formats[0];
	for (const VkSurfaceFormatKHR& format : formats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			surfaceFormat = format;
			break;
		}
	}

	VkExtent2D extent = capabilities.currentExtent;
	if (extent.width == UINT32_MAX) {
		int width = 0, height = 0;
		SDL_GetWindowSizeInPixels(desc.window, &width, &height);
		extent.width = std::clamp((U32)width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = std::clamp((U32)height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}

	U32 imageCount = std::max(capabilities.minImageCount, desc.frameCount);
	if (capabilities.maxImageCount) imageCount = std::min(imageCount, capabilities.maxImageCount);
	VkSwapchainCreateInfoKHR createInfo{
		.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface          = m_surface,
		.minImageCount    = imageCount,
		.imageFormat      = surfaceFormat.format,
		.imageColorSpace  = surfaceFormat.colorSpace,
		.imageExtent      = extent,
		.imageArrayLayers = 1,
		.imageUsage       = ToVkImageUsage(desc.swapchainImageUsage),
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform     = capabilities.currentTransform,
		.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode      = desc.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR,
		.clipped          = VK_TRUE,
	};

	VK_TRY(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain));
	VK_TRY(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr));

	std::vector<VkImage> images(imageCount);
	VK_TRY(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, images.data()));

	Format swapchainFormat = FromVkFormat(surfaceFormat.format);
	if (swapchainFormat == Format::None) return Err("Unsupported swapchain format: %u", (U32)surfaceFormat.format);
	for (VkImage vkImage : images) {
		Image* image = CreateImage(ImageDesc{
			.width            = extent.width,
			.height           = extent.height,
			.format           = swapchainFormat,
			.usage            = desc.swapchainImageUsage,
			.ownedBySwapchain = true,
			.existingImage    = vkImage,
		});
		if (!image) return Err("Failed to create swapchain image view");
		m_swapchainImages.push_back(image);
	}
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
		std::vector<const char*> instance_extensions(sdlExtensions, sdlExtensions + instanceExtensionCount);

		const char* validationLayer = "VK_LAYER_KHRONOS_validation";
		std::vector<const char*> layers;
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

		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
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
		TRY(CreateSwapchain(desc));
		TRY(CreateSemaphore(&m_renderDoneSemaphore));
		TRY(CreateSemaphore(&m_imageAcquiredSemaphore));
		m_frameFence = CreateFence(true);
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
	DestroyFence(m_frameFence);
	if (m_renderDoneSemaphore) vkDestroySemaphore(m_device, m_renderDoneSemaphore, nullptr);
	if (m_imageAcquiredSemaphore) vkDestroySemaphore(m_device, m_imageAcquiredSemaphore, nullptr);
	for (Image* image : m_swapchainImages) DestroyImage(image);
	m_swapchainImages.clear();
	if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	delete m_mainCommandBuffer;
	delete m_transferCommandBuffer;
	if (m_mainCommandPool) vkDestroyCommandPool(m_device, m_mainCommandPool, nullptr);
	if (m_transferCommandPool) vkDestroyCommandPool(m_device, m_transferCommandPool, nullptr);
	if (m_allocator) vmaDestroyAllocator(m_allocator);
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
		ImageBarrier(GFX::ImageBarrier{
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
		ImageBarrier(GFX::ImageBarrier{
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
		.pStencilAttachment   = desc.depthAttachment && HasStencil(desc.depthAttachment->image->m_format) ? &depthAttachment : nullptr,
	};
	vkCmdBeginRendering(m_vk, &renderingInfo);

	VkViewport viewport{ 0.0f, 0.0f, (float)extentImage->m_width, (float)extentImage->m_height, 0.0f, 1.0f, };
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
			ImageBarrier(GFX::ImageBarrier{
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
			ImageBarrier(GFX::ImageBarrier{
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

void CommandBuffer_Vulkan::CopyBuffer(GPUBuffer* source, GPUBuffer* destination, const BufferCopy& copy) {
	VkBufferCopy region{
		.srcOffset = copy.sourceOffset,
		.dstOffset = copy.destOffset,
		.size = copy.size,
	};
	vkCmdCopyBuffer(m_vk, source->m_vk, destination->m_vk, 1, &region);
}

void CommandBuffer_Vulkan::CopyBufferToImage(GPUBuffer* source, Image* destination, const BufferImageCopy& copy) {
	VkBufferImageCopy region{
		.bufferOffset = copy.bufferOffset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask = (VkImageAspectFlags)(IsDepthFormat(destination->m_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
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

void CommandBuffer_Vulkan::ImageBarrier(const GFX::ImageBarrier& barrier) {
	Image* image = barrier.image;
	VkImageAspectFlags aspect = IsDepthFormat(image->m_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	if (HasStencil(image->m_format)) aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	VkImageMemoryBarrier2 imageBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = ImageStateToPipelineStage(barrier.stateBefore),
		.srcAccessMask = ImageStateToAccessFlags(barrier.stateBefore),
		.dstStageMask = ImageStateToPipelineStage(barrier.stateAfter),
		.dstAccessMask = ImageStateToAccessFlags(barrier.stateAfter),
		.oldLayout = ImageStateToImageLayout(barrier.stateBefore),
		.newLayout = ImageStateToImageLayout(barrier.stateAfter),
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image->m_vulkan.image,
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

bool GraphicsDevice_Vulkan::BeginFrameImpl() {
	WaitForFence(m_frameFence);
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAcquiredSemaphore, VK_NULL_HANDLE, &m_swapchainImageIndex);
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		return false;
	}
	return (bool)BeginFrameCommandBuffers();
}

void GraphicsDevice_Vulkan::EndFrame() {
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
		.semaphore = m_renderDoneSemaphore,
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
	ResetFence(m_frameFence);
	vkQueueSubmit2(m_graphicsQueue, 1, &mainSubmit, m_frameFence->m_vk);

	VkPresentInfoKHR presentInfo{
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores    = &m_renderDoneSemaphore,
		.swapchainCount     = 1,
		.pSwapchains        = &m_swapchain,
		.pImageIndices      = &m_swapchainImageIndex,
	};
	vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
	m_frameCommandBuffersBegun = false;
}

CommandBuffer* GraphicsDevice_Vulkan::GetMainCommandBuffer() { return m_mainCommandBuffer; }

CommandBuffer* GraphicsDevice_Vulkan::GetTransferCommandBuffer() { return m_transferCommandBuffer; }

Image* GraphicsDevice_Vulkan::GetCurrentBackbuffer() { return m_swapchainImages.empty() ? nullptr : m_swapchainImages[m_swapchainImageIndex]; }

void GraphicsDevice_Vulkan::SetVSync(bool) {
	printf("STUB: SetVSync\n");
}

void GraphicsDevice_Vulkan::WaitIdle() { assert(0); }

CommandBuffer* GraphicsDevice_Vulkan::CreateCommandBuffer() { assert(0); return {}; }

void GraphicsDevice_Vulkan::DestroyCommandBuffer(CommandBuffer*) { assert(0); }

void GraphicsDevice_Vulkan::BeginCommandBuffer(CommandBuffer*) { assert(0); }

void GraphicsDevice_Vulkan::EndCommandBuffer(CommandBuffer*) { assert(0); }

Fence* GraphicsDevice_Vulkan::CreateFence(bool signaled) {
	VkFenceCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = signaled ? (VkFenceCreateFlags)VK_FENCE_CREATE_SIGNALED_BIT : 0,
	};
	Fence* fence = new Fence();
	if (vkCreateFence(m_device, &createInfo, nullptr, &fence->m_vk) != VK_SUCCESS) {
		delete fence;
		return nullptr;
	}
	return fence;
}

void GraphicsDevice_Vulkan::DestroyFence(Fence* fence) {
	if (!fence) return;
	if (fence->m_vk) vkDestroyFence(m_device, fence->m_vk, nullptr);
	delete fence;
}

void GraphicsDevice_Vulkan::WaitForFence(Fence* fence) {
	vkWaitForFences(m_device, 1, &fence->m_vk, VK_TRUE, UINT64_MAX);
}

void GraphicsDevice_Vulkan::ResetFence(Fence* fence) {
	vkResetFences(m_device, 1, &fence->m_vk);
}

void GraphicsDevice_Vulkan::Submit(const SubmitDesc& desc) {
	std::vector<VkCommandBufferSubmitInfo> commandBuffers(desc.commandBufferCount);
	for (U32 i = 0; i < desc.commandBufferCount; i++) {
		commandBuffers[i] = VkCommandBufferSubmitInfo{
			.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = desc.commandBuffers[i]->m_vk,
		};
	}
	VkSubmitInfo2 submitInfo{
		.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.commandBufferInfoCount = desc.commandBufferCount,
		.pCommandBufferInfos    = commandBuffers.data(),
	};
	vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, desc.fence ? desc.fence->m_vk : VK_NULL_HANDLE);
}

GPUBuffer* GraphicsDevice_Vulkan::CreateGPUBuffer(const GPUBufferDesc& desc) {
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
	VkResult result = vmaCreateBuffer(m_allocator, &bufferCreateInfo, &allocationCreateInfo,
		&buffer->m_vk, &buffer->m_vmaAllocation, &allocationInfo);
	if (result != VK_SUCCESS) {
		delete buffer;
		return nullptr;
	}
	buffer->m_size = desc.size;
	buffer->m_mapped = allocationInfo.pMappedData;
	return buffer;
}

void GraphicsDevice_Vulkan::DestroyGPUBuffer(GPUBuffer* buffer) {
	if (!buffer) return;
	vmaDestroyBuffer(m_allocator, buffer->m_vk, buffer->m_vmaAllocation);
	delete buffer;
}

Image* GraphicsDevice_Vulkan::CreateImage(const ImageDesc& desc) {
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

	Image* image = new Image();
	VkResult result = VK_SUCCESS;
	if (desc.existingImage) {
		image->m_vulkan.image = desc.existingImage;
	} else {
		result = vmaCreateImage(m_allocator, &imageCreateInfo, &allocationCreateInfo,
			&image->m_vulkan.image, &image->m_vulkan.allocation, nullptr);
		if (result != VK_SUCCESS) {
			delete image;
			return nullptr;
		}
	}

	VkImageAspectFlags aspect = IsDepthFormat(desc.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	if (HasStencil(desc.format)) aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
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
	return image;
}

void GraphicsDevice_Vulkan::DestroyImage(Image* image) {
	if (!image) return;
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
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
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
	return sampler;
}

void GraphicsDevice_Vulkan::DestroySampler(Sampler* sampler) {
	if (!sampler) return;
	if (sampler->m_vk) vkDestroySampler(m_device, sampler->m_vk, nullptr);
	delete sampler;
}

ShaderModule* GraphicsDevice_Vulkan::CreateShaderModule(const ShaderModuleDesc&) { assert(0); return {}; }

void GraphicsDevice_Vulkan::DestroyShaderModule(ShaderModule*) { assert(0); }

GraphicsPipeline* GraphicsDevice_Vulkan::CreateGraphicsPipeline(const GraphicsPipelineDesc&) { assert(0); return {}; }

void GraphicsDevice_Vulkan::DestroyGraphicsPipeline(GraphicsPipeline*) { assert(0); }

}
