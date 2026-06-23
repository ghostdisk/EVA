#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/GL.hpp>
#include <SDL3/SDL.h>
#include <string.h>

int vsync = 0;

int Con_vsync(int argc, const char** argv)
{
	if (argc < 2)
	{
		ConLog("usage: vsync 0|1");
		return -1;
	}

	vsync = 0;
	if (strcmp(argv[1], "1") == 0) vsync = 1;
	if (strcmp(argv[1], "2") == 0) vsync = 2;
	if (strcmp(argv[1], "3") == 0) vsync = 3;
	if (strcmp(argv[1], "4") == 0) vsync = 4;

	SDL_GL_SetSwapInterval(vsync);
	return 0;
}

void PlatformInitialize()
{
	ConRegisterCommand("vsync", Con_vsync, "toggle vsync");
}