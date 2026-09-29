#include <EVA/GPU/GPU_Vulkan.hpp>
#include <EVA/PAL/PAL.hpp>
#include <volk.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef EVA_WIN32
#include <Windows.h>
#endif
#ifdef EVA_ANDROID
#include <android/native_window.h>
#endif

#define VK_ASSERT(expr)                                                                                                 \
	do                                                                                                                  \
	{                                                                                                                   \
		VkResult vk_assert_result = (expr);                                                                             \
		if (vk_assert_result != VK_SUCCESS)                                                                             \
		{                                                                                                               \
			fprintf(stderr, "%s failed with VkResult %d at %s:%d\n", #expr, (int)vk_assert_result, __FILE__, __LINE__); \
			exit(1);                                                                                                    \
		}                                                                                                               \
	} while (0)

namespace EVA::GPU::Vulkan
{

struct VulkanTexture
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	TextureDesc desc;
	ImageState state = ImageState::UNDEFINED;
	bool owned_by_swapchain = false;
};

struct VulkanRenderPass
{
	VkRenderPass handle = VK_NULL_HANDLE;
	std::vector<AttachmentDesc> attachments;
};

struct VulkanFramebuffer
{
	VkFramebuffer handle = VK_NULL_HANDLE;
	std::vector<VulkanTexture*> attachments;
	uint32 width = 0;
	uint32 height = 0;
	uint32 layers = 1;
};

struct PhysicalDeviceProps
{
	VkPhysicalDevice device = VK_NULL_HANDLE;
	VkSurfaceFormatKHR format = {};
	uint32 graphics_family = UINT32_MAX;
	int32 score = -1;
};

static VkInstance instance = VK_NULL_HANDLE;
static VkSurfaceKHR surface = VK_NULL_HANDLE;
static PhysicalDeviceProps physical_device;
static VkDevice device = VK_NULL_HANDLE;
static VkQueue graphics_queue = VK_NULL_HANDLE;
static VkSwapchainKHR swapchain = VK_NULL_HANDLE;
static std::vector<VulkanTexture> backbuffers;
static uint32 current_backbuffer = 0;
static VkCommandPool command_pool = VK_NULL_HANDLE;
static VkCommandBuffer command_buffer = VK_NULL_HANDLE;
static VkSemaphore image_available = VK_NULL_HANDLE;
static std::vector<VkSemaphore> render_finished;
static VkFence submit_fence = VK_NULL_HANDLE;
static VulkanRenderPass* active_render_pass = nullptr;
static VulkanFramebuffer* active_framebuffer = nullptr;
static bool volk_initialized = false;

static VulkanTexture* ToImpl(Texture* texture)
{
	return reinterpret_cast<VulkanTexture*>(texture);
}
static VulkanRenderPass* ToImpl(RenderPass* render_pass)
{
	return reinterpret_cast<VulkanRenderPass*>(render_pass);
}
static VulkanFramebuffer* ToImpl(Framebuffer* framebuffer)
{
	return reinterpret_cast<VulkanFramebuffer*>(framebuffer);
}

static VkFormat ToVkFormat(TextureFormat format)
{
	switch (format)
	{
	case TextureFormat::RGBA8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case TextureFormat::BGRA8_UNORM:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case TextureFormat::D24_UNORM_S8_UINT:
		return VK_FORMAT_D24_UNORM_S8_UINT;
	}
	return VK_FORMAT_UNDEFINED;
}

static VkImageAspectFlags ImageAspect(TextureFormat format)
{
	return format == TextureFormat::D24_UNORM_S8_UINT
			   ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
			   : VK_IMAGE_ASPECT_COLOR_BIT;
}

static VkImageLayout ImageLayout(ImageState state)
{
	switch (state)
	{
	case ImageState::UNDEFINED:
		return VK_IMAGE_LAYOUT_UNDEFINED;
	case ImageState::COLOR_ATTACHMENT:
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case ImageState::DEPTH_STENCIL_ATTACHMENT:
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case ImageState::SHADER_READ:
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case ImageState::PRESENT:
		return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	return VK_IMAGE_LAYOUT_UNDEFINED;
}

static VkPipelineStageFlags ImageStage(ImageState state)
{
	switch (state)
	{
	case ImageState::UNDEFINED:
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	case ImageState::COLOR_ATTACHMENT:
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	case ImageState::DEPTH_STENCIL_ATTACHMENT:
		return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	case ImageState::SHADER_READ:
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	case ImageState::PRESENT:
		return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
	return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
}

static VkAccessFlags ImageAccess(ImageState state)
{
	switch (state)
	{
	case ImageState::COLOR_ATTACHMENT:
		return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case ImageState::DEPTH_STENCIL_ATTACHMENT:
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case ImageState::SHADER_READ:
		return VK_ACCESS_SHADER_READ_BIT;
	default:
		return 0;
	}
}

static VkImageUsageFlags ImageUsage(ImageState state)
{
	switch (state)
	{
	case ImageState::COLOR_ATTACHMENT:
		return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	case ImageState::DEPTH_STENCIL_ATTACHMENT:
		return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	case ImageState::SHADER_READ:
		return VK_IMAGE_USAGE_SAMPLED_BIT;
	default:
		return 0;
	}
}

static VkImageSubresourceRange Subresource(const VulkanTexture& texture)
{
	return VkImageSubresourceRange{
		.aspectMask = ImageAspect(texture.desc.format),
		.levelCount = texture.desc.mip_levels,
		.layerCount = texture.desc.layers,
	};
}

static void ImageBarrier(VkImage image, VkImageSubresourceRange subresource, ImageState before, ImageState after)
{
	auto barrier = VkImageMemoryBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = ImageAccess(before),
		.dstAccessMask = ImageAccess(after),
		.oldLayout = ImageLayout(before),
		.newLayout = ImageLayout(after),
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = subresource,
	};
	vkCmdPipelineBarrier(command_buffer, ImageStage(before), ImageStage(after), 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

static void DestroyTexture(VulkanTexture& texture)
{
	if (texture.view)
		vkDestroyImageView(device, texture.view, nullptr);
	if (texture.image && !texture.owned_by_swapchain)
		vkDestroyImage(device, texture.image, nullptr);
	texture = {};
}

static uint32 GetBackbufferCount()
{
	return (uint32)backbuffers.size();
}

static Texture* GetBackbuffer(uint32 index)
{
	return index < backbuffers.size() ? reinterpret_cast<Texture*>(&backbuffers[index]) : nullptr;
}

static TextureDesc GetTextureDesc(Texture* texture)
{
	return texture ? ToImpl(texture)->desc : TextureDesc{};
}

static bool BeginFrame()
{
	VK_ASSERT(vkWaitForFences(device, 1, &submit_fence, VK_TRUE, UINT64_MAX));
	VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
		image_available, VK_NULL_HANDLE, &current_backbuffer);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
		return false;
	if (result != VK_SUBOPTIMAL_KHR)
		VK_ASSERT(result);
	VK_ASSERT(vkResetCommandPool(device, command_pool, 0));
	auto begin_info = VkCommandBufferBeginInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_ASSERT(vkBeginCommandBuffer(command_buffer, &begin_info));
	return true;
}

static Texture* GetCurrentBackbuffer()
{
	return GetBackbuffer(current_backbuffer);
}

static VkAttachmentLoadOp ToVkLoadOp(AttachmentLoadOp op)
{
	switch (op)
	{
	case AttachmentLoadOp::LOAD:
		return VK_ATTACHMENT_LOAD_OP_LOAD;
	case AttachmentLoadOp::CLEAR:
		return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case AttachmentLoadOp::DONT_CARE:
		return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}
	return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

static VkAttachmentStoreOp ToVkStoreOp(AttachmentStoreOp op)
{
	return op == AttachmentStoreOp::STORE ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

static RenderPass* CreateRenderPass(const RenderPassDesc& desc)
{
	std::vector<VkAttachmentDescription> attachments(desc.attachments.count);
	std::vector<VkAttachmentReference> color_references(desc.attachments.count);
	VkAttachmentReference depth_reference = {};
	uint32 color_count = 0;
	bool has_depth = false;
	for (uint32 i = 0; i < desc.attachments.count; ++i)
	{
		const AttachmentDesc& attachment = desc.attachments[i];
		VkFormat format = ToVkFormat(attachment.format);
		if (attachment.state_during == ImageState::COLOR_ATTACHMENT)
		{
			color_references[color_count++] = VkAttachmentReference{
				.attachment = i,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			};
		}
		else if (attachment.state_during == ImageState::DEPTH_STENCIL_ATTACHMENT)
		{
			depth_reference = VkAttachmentReference{
				.attachment = i,
				.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			};
			has_depth = true;
		}
		attachments[i] = VkAttachmentDescription{
			.format = format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = ToVkLoadOp(attachment.load_op),
			.storeOp = ToVkStoreOp(attachment.store_op),
			.stencilLoadOp = attachment.format == TextureFormat::D24_UNORM_S8_UINT ? ToVkLoadOp(attachment.load_op) : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = attachment.format == TextureFormat::D24_UNORM_S8_UINT ? ToVkStoreOp(attachment.store_op) : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = ImageLayout(attachment.state_during),
			.finalLayout = ImageLayout(attachment.state_during),
		};
	}
	auto subpass = VkSubpassDescription{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = color_count,
		.pColorAttachments = color_count ? color_references.data() : nullptr,
		.pDepthStencilAttachment = has_depth ? &depth_reference : nullptr,
	};
	auto create_info = VkRenderPassCreateInfo{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = desc.attachments.count,
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
	};
	auto* render_pass = new VulkanRenderPass;
	VK_ASSERT(vkCreateRenderPass(device, &create_info, nullptr, &render_pass->handle));
	render_pass->attachments.assign(desc.attachments.data, desc.attachments.data + desc.attachments.count);
	return reinterpret_cast<RenderPass*>(render_pass);
}

static void DestroyRenderPass(RenderPass* render_pass)
{
	if (!render_pass)
		return;
	auto* impl = ToImpl(render_pass);
	VK_ASSERT(vkDeviceWaitIdle(device));
	vkDestroyRenderPass(device, impl->handle, nullptr);
	delete impl;
}

static Framebuffer* CreateFramebuffer(FramebufferDesc&& desc)
{
	auto* render_pass = ToImpl(desc.render_pass);
	std::vector<VkImageView> views(desc.attachments.count);
	auto* framebuffer = new VulkanFramebuffer;
	framebuffer->attachments.resize(desc.attachments.count);
	for (uint32 i = 0; i < desc.attachments.count; ++i)
	{
		auto* texture = ToImpl(desc.attachments[i]);
		if (i == 0)
		{
			framebuffer->width = texture->desc.width;
			framebuffer->height = texture->desc.height;
			framebuffer->layers = texture->desc.layers;
		}
		views[i] = texture->view;
		framebuffer->attachments[i] = texture;
	}
	auto create_info = VkFramebufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = render_pass->handle,
		.attachmentCount = desc.attachments.count,
		.pAttachments = views.data(),
		.width = framebuffer->width,
		.height = framebuffer->height,
		.layers = framebuffer->layers,
	};
	VK_ASSERT(vkCreateFramebuffer(device, &create_info, nullptr, &framebuffer->handle));
	return reinterpret_cast<Framebuffer*>(framebuffer);
}

static void DestroyFramebuffer(Framebuffer* framebuffer)
{
	if (!framebuffer)
		return;
	auto* impl = ToImpl(framebuffer);
	VK_ASSERT(vkDeviceWaitIdle(device));
	vkDestroyFramebuffer(device, impl->handle, nullptr);
	delete impl;
}

static void BeginRenderPass(const RenderPassBeginDesc& desc)
{
	auto* render_pass = ToImpl(desc.render_pass);
	auto* framebuffer = ToImpl(desc.framebuffer);
	std::vector<VkClearValue> clear_values(render_pass->attachments.size());
	for (uint32 i = 0; i < render_pass->attachments.size(); ++i)
	{
		const AttachmentDesc& attachment = render_pass->attachments[i];
		VulkanTexture* texture = framebuffer->attachments[i];
		if (desc.clear_values.data && i < desc.clear_values.count)
		{
			for (uint32 component = 0; component < 4; ++component)
				clear_values[i].color.float32[component] = desc.clear_values[i].color[component];
			if (attachment.state_during == ImageState::DEPTH_STENCIL_ATTACHMENT)
			{
				clear_values[i].depthStencil.depth = desc.clear_values[i].depth;
				clear_values[i].depthStencil.stencil = desc.clear_values[i].stencil;
			}
		}
		ImageBarrier(texture->image, Subresource(*texture), texture->state, attachment.state_during);
		texture->state = attachment.state_during;
	}
	auto begin_info = VkRenderPassBeginInfo{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = render_pass->handle,
		.framebuffer = framebuffer->handle,
		.renderArea = { .extent = { framebuffer->width, framebuffer->height } },
		.clearValueCount = (uint32)render_pass->attachments.size(),
		.pClearValues = clear_values.data(),
	};
	vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	active_render_pass = render_pass;
	active_framebuffer = framebuffer;
}

static void EndRenderPass()
{
	vkCmdEndRenderPass(command_buffer);
	for (uint32 i = 0; i < active_render_pass->attachments.size(); ++i)
	{
		VulkanTexture* texture = active_framebuffer->attachments[i];
		ImageState after = active_render_pass->attachments[i].state_after;
		ImageBarrier(texture->image, Subresource(*texture), texture->state, after);
		texture->state = after;
	}
	active_render_pass = nullptr;
	active_framebuffer = nullptr;
}

static bool EndFrame()
{
	VK_ASSERT(vkEndCommandBuffer(command_buffer));
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	auto submit_info = VkSubmitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &image_available,
		.pWaitDstStageMask = &wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &command_buffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &render_finished[current_backbuffer],
	};
	VK_ASSERT(vkResetFences(device, 1, &submit_fence));
	VK_ASSERT(vkQueueSubmit(graphics_queue, 1, &submit_info, submit_fence));
	auto present_info = VkPresentInfoKHR{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &render_finished[current_backbuffer],
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &current_backbuffer,
	};
	VkResult result = vkQueuePresentKHR(graphics_queue, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
		return false;
	if (result != VK_SUBOPTIMAL_KHR)
		VK_ASSERT(result);
	return true;
}

static void Shutdown()
{
	if (device)
		VK_ASSERT(vkDeviceWaitIdle(device));
	for (VkSemaphore semaphore : render_finished)
	{
		if (semaphore)
			vkDestroySemaphore(device, semaphore, nullptr);
	}
	render_finished.clear();
	if (image_available)
		vkDestroySemaphore(device, image_available, nullptr);
	if (submit_fence)
		vkDestroyFence(device, submit_fence, nullptr);
	if (command_pool)
		vkDestroyCommandPool(device, command_pool, nullptr);
	for (VulkanTexture& texture : backbuffers)
		DestroyTexture(texture);
	backbuffers.clear();
	if (swapchain)
		vkDestroySwapchainKHR(device, swapchain, nullptr);
	if (device)
		vkDestroyDevice(device, nullptr);
	if (surface)
		vkDestroySurfaceKHR(instance, surface, nullptr);
	if (instance)
		vkDestroyInstance(instance, nullptr);
	if (volk_initialized)
		volkFinalize();
	instance = VK_NULL_HANDLE;
	surface = VK_NULL_HANDLE;
	physical_device = {};
	device = VK_NULL_HANDLE;
	graphics_queue = VK_NULL_HANDLE;
	swapchain = VK_NULL_HANDLE;
	command_pool = VK_NULL_HANDLE;
	command_buffer = VK_NULL_HANDLE;
	image_available = VK_NULL_HANDLE;
	submit_fence = VK_NULL_HANDLE;
	active_render_pass = nullptr;
	active_framebuffer = nullptr;
	volk_initialized = false;
}

static bool SupportsSwapchain(VkPhysicalDevice candidate)
{
	uint32 count = 0;
	VK_ASSERT(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr));
	if (!count)
		return false;
	std::vector<VkExtensionProperties> extensions(count);
	VK_ASSERT(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, extensions.data()));
	for (uint32 i = 0; i < count; ++i)
	{
		if (std::strcmp(extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
			return true;
	}
	return false;
}

static int32 DeviceScore(VkPhysicalDeviceType type)
{
	switch (type)
	{
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return 2;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return 1;
	default:
		return 0;
	}
}

static bool ChoosePhysicalDevice()
{
	uint32 count = 0;
	VK_ASSERT(vkEnumeratePhysicalDevices(instance, &count, nullptr));
	if (!count)
		return false;

	std::vector<VkPhysicalDevice> candidates(count);
	VK_ASSERT(vkEnumeratePhysicalDevices(instance, &count, candidates.data()));
	for (uint32 candidate_index = 0; candidate_index < count; ++candidate_index)
	{
		VkPhysicalDevice candidate = candidates[candidate_index];
		if (!SupportsSwapchain(candidate))
			continue;
		PhysicalDeviceProps props;
		props.device = candidate;
		uint32 family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
		if (!family_count)
			continue;
		std::vector<VkQueueFamilyProperties> families(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
		for (uint32 i = 0; i < family_count; ++i)
		{
			if (!families[i].queueCount || !(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
				continue;
			VkBool32 supports_present = VK_FALSE;
			VK_ASSERT(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &supports_present));
			if (supports_present)
			{
				props.graphics_family = i;
				break;
			}
		}
		if (props.graphics_family == UINT32_MAX)
			continue;
		uint32 format_count = 0;
		uint32 mode_count = 0;
		VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &format_count, nullptr));
		VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &mode_count, nullptr));
		if (!format_count || !mode_count)
			continue;
		std::vector<VkSurfaceFormatKHR> formats(format_count);
		VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &format_count, formats.data()));
		for (uint32 i = 0; i < format_count; ++i)
		{
			if (formats[i].colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				continue;
			if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || formats[i].format == VK_FORMAT_UNDEFINED)
			{
				props.format = VkSurfaceFormatKHR{
					.format = VK_FORMAT_R8G8B8A8_UNORM,
					.colorSpace = formats[i].colorSpace,
				};
				break;
			}
			if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
				props.format = formats[i];
		}
		if (props.format.format == VK_FORMAT_UNDEFINED)
			continue;
		VkPhysicalDeviceProperties device_properties = {};
		vkGetPhysicalDeviceProperties(candidate, &device_properties);
		props.score = DeviceScore(device_properties.deviceType);
		if (props.score > physical_device.score)
			physical_device = props;
	}
	return physical_device.device != VK_NULL_HANDLE;
}

static bool CreateSwapchain()
{
	VkSurfaceCapabilitiesKHR capabilities = {};
	VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device.device, surface, &capabilities));
	if (!(capabilities.supportedUsageFlags & ImageUsage(ImageState::COLOR_ATTACHMENT)))
		return false;
	TextureFormat texture_format = physical_device.format.format == VK_FORMAT_R8G8B8A8_UNORM
									   ? TextureFormat::RGBA8_UNORM
									   : TextureFormat::BGRA8_UNORM;
	VkExtent2D extent = capabilities.currentExtent;
	if (!extent.width || !extent.height)
		return false;
	uint32 requested_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount && requested_count > capabilities.maxImageCount)
		requested_count = capabilities.maxImageCount;

	auto create_info = VkSwapchainCreateInfoKHR{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = requested_count,
		.imageFormat = physical_device.format.format,
		.imageColorSpace = physical_device.format.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = ImageUsage(ImageState::COLOR_ATTACHMENT),
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
							  ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
							  : (VkCompositeAlphaFlagBitsKHR)(capabilities.supportedCompositeAlpha & -capabilities.supportedCompositeAlpha),
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.clipped = VK_TRUE,
	};
	VK_ASSERT(vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain));
	uint32 image_count = 0;
	VK_ASSERT(vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr));
	std::vector<VkImage> images(image_count);
	VK_ASSERT(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images.data()));
	backbuffers.resize(image_count);
	for (uint32 i = 0; i < image_count; ++i)
	{
		VulkanTexture& texture = backbuffers[i];
		texture.image = images[i];
		texture.owned_by_swapchain = true;
		texture.desc.width = extent.width;
		texture.desc.height = extent.height;
		texture.desc.format = texture_format;
		auto view_info = VkImageViewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = texture.image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = physical_device.format.format,
			.subresourceRange = Subresource(texture),
		};
		VK_ASSERT(vkCreateImageView(device, &view_info, nullptr, &texture.view));
	}
	return true;
}

static bool InitImpl(const InitOptions& init_options)
{
	if (!init_options.window || !init_options.window->native_handle)
		return false;
	VK_ASSERT(volkInitialize());
	volk_initialized = true;

	{ // create instance:
		auto application_info = VkApplicationInfo{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "EVA",
			.apiVersion = VK_API_VERSION_1_0,
		};
		const char* instance_extensions[] = {
			VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef EVA_WIN32
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#ifdef EVA_ANDROID
			VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#endif
		};
		auto instance_info = VkInstanceCreateInfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &application_info,
			.enabledExtensionCount = sizeof(instance_extensions) / sizeof(instance_extensions[0]),
			.ppEnabledExtensionNames = instance_extensions,
		};
		VK_ASSERT(vkCreateInstance(&instance_info, nullptr, &instance));
		volkLoadInstance(instance);
	}

	{ // create surface:
#ifdef EVA_WIN32
		auto surface_info = VkWin32SurfaceCreateInfoKHR{
			.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.hinstance = GetModuleHandleA(nullptr),
			.hwnd = static_cast<HWND>(init_options.window->native_handle),
		};
		VK_ASSERT(vkCreateWin32SurfaceKHR(instance, &surface_info, nullptr, &surface));
#elif defined(EVA_ANDROID)
		auto surface_info = VkAndroidSurfaceCreateInfoKHR{
			.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
			.window = static_cast<ANativeWindow*>(init_options.window->native_handle),
		};
		VK_ASSERT(vkCreateAndroidSurfaceKHR(instance, &surface_info, nullptr, &surface));
#else
		return false;
#endif
	}

	{ // pick physical device:
		if (!ChoosePhysicalDevice())
			return false;
	}

	{ // create device:
		float priority = 1.0f;
		auto queue_info = VkDeviceQueueCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = physical_device.graphics_family,
			.queueCount = 1,
			.pQueuePriorities = &priority,
		};
		const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		auto device_info = VkDeviceCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queue_info,
			.enabledExtensionCount = 1,
			.ppEnabledExtensionNames = device_extensions,
		};
		VK_ASSERT(vkCreateDevice(physical_device.device, &device_info, nullptr, &device));
		volkLoadDevice(device);
		vkGetDeviceQueue(device, physical_device.graphics_family, 0, &graphics_queue);
	}

	{ // create swpahcain:
		if (!CreateSwapchain())
			return false;
	}

	{ // create command pool and buffers:
		auto pool_info = VkCommandPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = physical_device.graphics_family,
		};
		VK_ASSERT(vkCreateCommandPool(device, &pool_info, nullptr, &command_pool));
		auto allocate_info = VkCommandBufferAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = command_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		VK_ASSERT(vkAllocateCommandBuffers(device, &allocate_info, &command_buffer));
	}

	{ // create sync resources:
		auto semaphore_info = VkSemaphoreCreateInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		VK_ASSERT(vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available));
		render_finished.resize(backbuffers.size(), VK_NULL_HANDLE);
		for (VkSemaphore& semaphore : render_finished)
		{
			VK_ASSERT(vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore));
		}
		auto fence_info = VkFenceCreateInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		VK_ASSERT(vkCreateFence(device, &fence_info, nullptr, &submit_fence));
	}

	return true;
}

static bool Init(Device& out_device, const InitOptions& init_options)
{
	if (!InitImpl(init_options))
	{
		Shutdown();
		return false;
	}
	out_device = Device{
		.Shutdown = Shutdown,
		.CreateRenderPass = CreateRenderPass,
		.DestroyRenderPass = DestroyRenderPass,
		.CreateFramebuffer = CreateFramebuffer,
		.DestroyFramebuffer = DestroyFramebuffer,
		.GetBackbufferCount = GetBackbufferCount,
		.GetBackbuffer = GetBackbuffer,
		.GetTextureDesc = GetTextureDesc,
		.BeginFrame = BeginFrame,
		.GetCurrentBackbuffer = GetCurrentBackbuffer,
		.BeginRenderPass = BeginRenderPass,
		.EndRenderPass = EndRenderPass,
		.EndFrame = EndFrame,
	};
	return true;
}

BackendDesc backend_desc = {
	.backend = Backend::VULKAN,
	.Init = Init,
};

}
