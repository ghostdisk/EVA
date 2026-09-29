#include <EVA/GPU/GPU.hpp>
#include <EVA/PAL/PAL.hpp>

using namespace EVA;

PAL::Window window;
bool quit = false;

int main()
{
	PAL::Init();

	PAL::InitWindow(&window, {
		.name = "EVA Test App",
		.width = 800,
		.height = 600,
	});

	GPU::Init({
		.window = &window,
		.preferred_backend = GPU::Backend::D3D11,
	});

	while (!quit)
	{
		PAL::Event event;

		while (PAL::Poll(&event))
		{
			switch (event.type)
			{
			case PAL::EventType::CLOSE_REQUESTED:
			{
				quit = true;
				break;
			}
			}
		}
	}

	GPU::Shutdown();
	PAL::DeinitWindow(&window);

	return 0;
}
