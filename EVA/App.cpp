#include <EVA/App.hpp>
#include <EVA/Renderer/GL.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Input.hpp>
#include <EVA/Camera.hpp>
#include <EVA/Physics.hpp>
#include <EVA/UI.hpp>
#include <EVA/Net.hpp>
#include <EVA/Font.hpp>
#include <EVA/Camera.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Entities.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Game.hpp>
#include <EVA/Library.hpp>
#include <EVA/Editor.hpp>
#include <EVA/MainMenu.hpp>
#include <SDL3/SDL.h>
#include <enet/enet.h>
#include <tracy/Tracy.hpp>

AppMode g_app_mode = AppMode_None;

struct NextFrameCallback
{
	void (*callback)(void* userdata);
	void* userdata;
};

static std::vector<NextFrameCallback> next_frame_callbacks;


void QueueForNextFrame(void (*callback)(void* userdata), void* userdata)
{
	next_frame_callbacks.push_back({ callback, userdata });
}

void AppSetMode(AppMode mode, Game* game)
{
	g_app_mode = mode;

	switch (g_app_mode)
	{
		case AppMode_MainMenu:
		{
			break;
		}
		case AppMode_Editor:
		{
			assert(!game);
			g_active_game = nullptr;
			g_current_camera = &g_editor_camera;
			break;
		}
		case AppMode_Game:
		{
			assert(game);
			g_active_game = game;
			g_current_camera = &game->camera;
			break;
		}
		default: assert(0);
	}
}

int main()
{
	ArenaInitialize();
	RotateFrameArenas();
	PlatformInitialize();
	NetInitialize();
	ConsoleInitialize();
	GLInitialize();
	PhysicsInitialize();
	GameInitialize();
	RendererInitialize();
	FontInitialize();
	LibraryInitialize();
	InputInitialize();
	UIInitialize();
	EdInitialize();

	InputBindKey(InputAxis_Horizontal, SDL_SCANCODE_A, -1.0f);
	InputBindKey(InputAxis_Horizontal, SDL_SCANCODE_D,  1.0f);
	InputBindKey(InputAxis_Vertical,   SDL_SCANCODE_S, -1.0f);
	InputBindKey(InputAxis_Vertical,   SDL_SCANCODE_W,  1.0f);
	InputBindKey(InputAxis_Fly,        SDL_SCANCODE_Q, -1.0f);
	InputBindKey(InputAxis_Fly,        SDL_SCANCODE_E,  1.0f);

	AppSetMode(AppMode_MainMenu, nullptr);
	ConExec("exec autoexec.cfg");

	while (!g_quit)
	{
		RotateFrameArenas();
		PlatformBeginFrame();

		InputBeginFrame();
		UIBeginFrame();


		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (PlatformProcessSDLEvent(&event)) continue;
			if (UIProcessSDLEvent(&event)) continue;
			if (InputProcessSDLEvent(&event)) continue;
		}
		InputUpdateAxes();

		for (auto& cb : next_frame_callbacks) cb.callback(cb.userdata);
		next_frame_callbacks.clear();

		GameTickAll(g_delta_time);

		switch (g_app_mode)
		{
			case AppMode_Game:
			{
				GameDraw(g_active_game);
				break;
			}
			case AppMode_MainMenu:
			{
				MainMenuTick();
				break;
			}
			case AppMode_Editor:
			{
				EdTick();
				break;
			}
			default: break;
		}

		ConsoleDraw();
		UIEndFrame();
		UIDraw();

		RenderFrame();
		SDL_GL_SwapWindow(g_game_window);

		FrameMark;
	}

	return 0;
}