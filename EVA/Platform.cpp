#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/GL.hpp>
#include <SDL3/SDL.h>

int vsync = 0;

ConValue Con_vsync(int argc, ConValue* argv)
{
	if (argc >= 1 && argv[0].type == ConValueType_Number)
	{
		vsync = (int)argv[0].number;
	}
	SDL_GL_SetSwapInterval(vsync);
	return ConValue{
		.type = ConValueType_Number,
		.number = (float)vsync,
	};
}


void PlatformInitialize()
{
	ConRegisterCommand("vsync", Con_vsync, "toggle vsync");
}