#include <EVA/Renderer/GL.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <EVA/Net.hpp>
#include <EVA/Font.hpp>
#include <EVA/Camera.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Entities.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Game.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Library.hpp>
#include <EVA/Editor.hpp>
#include <SDL3/SDL.h>
#include <enet/enet.h>
#include <tracy/Tracy.hpp>

struct NextFrameCallback
{
	void (*callback)(void* userdata);
	void* userdata;
};

Font* fnt_arial = 0;
static std::vector<NextFrameCallback> next_frame_callbacks;

int main()
{
	PlatformInitialize();
	NetInitialize();
	ArenaInitialize();
	ConsoleInitialize();
	RotateFrameArenas();
	GLInitialize();
	GameInitialize();
	RendererInitialize();
	PhysicsInitialize();
	LibraryInitialize();
	InputInitialize();
	FontInitialize();
	UIInitialize();
	EdInitialize();

	InputBindKey(InputAxis_Horizontal, SDL_SCANCODE_A, -1.0f);
	InputBindKey(InputAxis_Horizontal, SDL_SCANCODE_D,  1.0f);
	InputBindKey(InputAxis_Vertical,   SDL_SCANCODE_S, -1.0f);
	InputBindKey(InputAxis_Vertical,   SDL_SCANCODE_W,  1.0f);
	InputBindKey(InputAxis_Fly,        SDL_SCANCODE_Q, -1.0f);
	InputBindKey(InputAxis_Fly,        SDL_SCANCODE_E,  1.0f);

	ConExec("exec autoexec.cfg");
	ConExec("game 0");

	while (!g_quit)
	{
		RotateFrameArenas();
		PlatformBeginFrame();

		InputBeginFrame();
		UIBeginFrame();

		for (auto& cb : next_frame_callbacks) cb.callback(cb.userdata);
		next_frame_callbacks.clear();

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (PlatformProcessSDLEvent(&event)) continue;
			if (UIProcessSDLEvent(&event)) continue;
			if (InputProcessSDLEvent(&event)) continue;
		}
		InputUpdateAxes();

		GameTickAll(g_delta_time);
		GameDraw(ActiveGame);
		ConsoleDraw();
		EdTick();
		UIEndFrame();
		UIDraw();

		RenderFrame();
		SDL_GL_SwapWindow(g_game_window);

		FrameMark;
	}

	return 0;
}

void QueueForNextFrame(void (*callback)(void* userdata), void* userdata)
{
	next_frame_callbacks.push_back({ callback, userdata });
}