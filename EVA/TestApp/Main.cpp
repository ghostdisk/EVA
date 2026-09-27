#include <EVA/GPU/GPU.hpp>
#include <EVA/PAL/PAL.hpp>

using namespace EVA;

PAL::Window window;

int main()
{
	PAL::Init();

	PAL::InitWindow(&window, {
		.name = "EVA Test App",
		.width = 800,
		.height = 600,
	});

	GPU::Init({
		.preferred_backend = GPU::Backend::VULKAN,
	});

	for (;;)
	{
		PAL::Event event;

		while (PAL::Poll(&event))
		{
		}
	}

	GPU::Shutdown();

	return 0;
}