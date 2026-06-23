#include <EVA/GL.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <EVA/Draw.hpp>
#include <EVA/Camera.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Entities.hpp>
#include <EVA/Renderer.hpp>
#include <EVA/ClientGame.hpp>
#include <EVA/ServerGame.hpp>
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
DrawContext DC;
Font* fnt_arial = 0;
float FrameTimeHistory[FRAME_TIME_HISTORY_SIZE] = {};
float FPS = 0;
std::vector<NextFrameCallback> next_frame_callbacks;

// time:
static U64 FrameStartTimeNS;
double DeltaTime = 0.01;

ServerGame* server = nullptr;
ClientGame* client = nullptr;

int main()
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		Fatal("SDL_Init: %s", SDL_GetError());
	}

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
	RendererInitialize();
	PhysicsInitialize();
	LibraryInitialize();
	InputInitialize();
	DrawInitialize();
	UIInitialize();
	EditorInitialize();

	InputBindKey(InputAxis_Horizontal, SDL_SCANCODE_A, -1.0f);
	InputBindKey(InputAxis_Horizontal, SDL_SCANCODE_D,  1.0f);
	InputBindKey(InputAxis_Vertical,   SDL_SCANCODE_S, -1.0f);
	InputBindKey(InputAxis_Vertical,   SDL_SCANCODE_W,  1.0f);
	InputBindKey(InputAxis_Fly,        SDL_SCANCODE_Q, -1.0f);
	InputBindKey(InputAxis_Fly,        SDL_SCANCODE_E,  1.0f);

	fnt_arial = FontLoad("Arial.ttf", 20, 512);

	DrawContextInit(DC);
	UIContextInit(main_ui, fnt_arial);

	server = new ServerGame();
	ServerGameInit(server, "SERVER");
	ServerListen(server, 27015);
	ActiveGame = server;

	// client = new ClientGame();
	// ClientGameInit(client, "CLIENT0");
	// ClientConnect(client, {127,0,0,1}, 27015);
	// ActiveGame = client;

	FrameStartTimeNS = SDL_GetTicksNS();
	ConExec("exec autoexec.cfg");

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
		RendererBeginFrame();

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

		if (InputGetButtonDown(SDL_SCANCODE_F1)) ActiveGame = server;
		if (InputGetButtonDown(SDL_SCANCODE_F2)) ActiveGame = client;
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

		EditorEarlyTick();
		if (client) ClientGameTick(client, DeltaTime);
		if (server) ServerGameTick(server, DeltaTime);
		GameDraw(ActiveGame);
		EditorLateTick();
		UIEndFrame();
		UIDraw(DC);

		{ // Render frame:
			glViewport(0, 0, WindowWidth, WindowHeight);
			float4 clear_color = COLOR_RGB(44, 82, 87);
			glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

			glEnable(GL_DEPTH_TEST);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDepthFunc(GL_LESS);
			glDisable(GL_BLEND);

			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);

			RenderScene();
			DrawRender(DC);

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