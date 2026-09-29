#include <EVA/GPU/GPU_Vulkan.hpp>
#include <volk.h>
#include <cassert>

namespace EVA::GPU::Vulkan
{

static bool Init(Device& out_device, const InitOptions& init_options)
{
	(void)out_device;
	(void)init_options;
	assert(0);
	return false;
}

BackendDesc backend_desc = {
	.backend = Backend::VULKAN,
	.Init = Init,
};

}
