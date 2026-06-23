#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/GL.hpp>
#include <SDL3/SDL.h>

// int vsync = 0;

ConVar vsync = {
	.name = "vsync",
	.help = "0 - no vsync, 1 - every vblank, 2 - every 2nd vblank",
	.value = {
		.type = ConValueType_Number,
		.number = 0,
	},
	.on_change = [](ConVar*)
		{
			SDL_GL_SetSwapInterval(vsync.value.number);
		},
};

void PlatformInitialize()
{
	ConRegisterVar(&vsync);
}