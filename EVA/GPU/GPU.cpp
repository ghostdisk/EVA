#include <EVA/GPU/GPU.hpp>
#ifdef EVA_METAL
#include <EVA/GPU/GPU_Metal.hpp>
#endif
#ifdef EVA_WIN32
#include <EVA/GPU/GPU_D3D11.hpp>
#endif
#ifdef EVA_VULKAN
#include <EVA/GPU/GPU_Vulkan.hpp>
#endif

#include <cstdlib>

namespace EVA::GPU
{

Device device;

BackendDesc* backend_descs[] = {
#ifdef EVA_METAL
	&Metal::backend_desc,
#endif
#ifdef EVA_VULKAN
	&Vulkan::backend_desc,
#endif
#ifdef EVA_WIN32
	&D3D11::backend_desc,
#endif
	nullptr
};

void Init(const InitOptions& init_options)
{
	BackendDesc* backend_desc = nullptr;

	for (BackendDesc** candidate = backend_descs; *candidate; ++candidate)
	{
		if (!backend_desc)
			backend_desc = *candidate;
		if ((*candidate)->backend == init_options.preferred_backend)
		{
			backend_desc = *candidate;
			break;
		}
	}

	if (!backend_desc || !backend_desc->Init(device, init_options))
		std::abort();
}

}
