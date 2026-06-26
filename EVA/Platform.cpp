#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/Renderer/GL.hpp>
#include <SDL3/SDL.h>

ConVar cvar_vsync = {
	.name = "vsync",
	.help = "0 - no vsync, 1 - every vblank, 2 - every 2nd vblank",
	.fvalue = 0,
	.on_change = [](ConVar*)
		{
			SDL_GL_SetSwapInterval((int)cvar_vsync.fvalue);
		},
};

void PlatformInitialize()
{
	ConRegisterVar(&cvar_vsync);
}