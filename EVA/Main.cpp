#include <EVA/Renderer/GL.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
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

#define FRAME_TIME_HISTORY_SIZE 50

struct NextFrameCallback
{
	void (*callback)(void* userdata);
	void* userdata;
};

SDL_Window* GameWindow = nullptr;
bool DoQuit = false;
int WindowWidth = 1600;
int WindowHeight = 900;
bool InMenu = false;
static UIContext main_ui;
Font* fnt_arial = 0;
float FrameTimeHistory[FRAME_TIME_HISTORY_SIZE] = {};
float FPS = 0;
std::vector<NextFrameCallback> next_frame_callbacks;

// time:
static U64 FrameStartTimeNS;
double DeltaTime = 0.01;

void FontInitialize();

int main()
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		Fatal("SDL_Init: %s", SDL_GetError());
	}

	GLPreInitialize();
	GameWindow = SDL_CreateWindow("EVA", WindowWidth, WindowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!GameWindow)
	{
		Fatal("SDL_CreateWindow: %s", SDL_GetError());
	}

	if (enet_initialize() != 0)
	{
		Fatal("enet_initialize failed");
	}

	ArenaInitialize();
	ConsoleInitialize();
	PlatformInitialize();
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

	fnt_arial = FontLoad("Arial.ttf", 20, 512);

	UIContextInit(main_ui, fnt_arial);

	FrameStartTimeNS = SDL_GetTicksNS();

	ConExec("exec autoexec.cfg");
	ConExec("game 0");

	while (!DoQuit)
	{
		U64 new_time = SDL_GetTicksNS();
		U64 dt_ns = new_time - FrameStartTimeNS;
		DeltaTime = double(dt_ns) / 1'000'000'000;
		FrameStartTimeNS = new_time;

		for (auto& cb : next_frame_callbacks)
		{
			cb.callback(cb.userdata);
		}
		next_frame_callbacks.clear();

		RotateFrameArenas();
		InputBeginFrame();
		UIContextMakeCurrent(main_ui);
		UIBeginFrame();

		SDL_GetWindowSize(GameWindow, &WindowWidth, &WindowHeight);

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (UIProcessSDLEvent(&event)) continue;
			if (InputProcessSDLEvent(&event)) continue;

			switch (event.type)
			{
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				{
					DoQuit = true;
					break;
				}
			}
		}
		InputUpdateAxes();

		if (InputGetButtonDown(SDL_SCANCODE_ESCAPE)) InMenu = !InMenu;

		{ // Update tracked FPS
			static int k = 0;
			FrameTimeHistory[k] = DeltaTime;
			k++;
			if (k >= FRAME_TIME_HISTORY_SIZE) k = 0;

			float avg = 0;
			for (int i = 0; i < EVA_ARRAYSIZE(FrameTimeHistory); i++)
			{
				avg += FrameTimeHistory[i];
			}
			avg = avg / (float)(FRAME_TIME_HISTORY_SIZE);
			FPS = 1.0f / avg;
		}

		GameTickAll(DeltaTime);
		GameDraw(ActiveGame);
		ConsoleDraw();
		EdTick();
		UIEndFrame();
		UIDraw();

		{ // Render frame:
			RenderFrame();

			DrawSprite(Library::spr_crosshair, WindowWidth/2 - 7, WindowHeight/2 - 7, COLOR_WHITE);
			GL_ERROR_CHECK();

			SDL_GL_SwapWindow(GameWindow);
		}

		FrameMark;
	}

	return 0;
}

void QueueForNextFrame(void (*callback)(void* userdata), void* userdata)
{
	next_frame_callbacks.push_back({ callback, userdata });
}