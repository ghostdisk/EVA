#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/GFX/GraphicsDevice.hpp>
#include <SDL3/SDL.h>

#define FRAME_TIME_HISTORY_SIZE 50

float2 g_window_size    = { 1600, 900 };
float  g_fps            = 0;
bool   g_quit           = false;
double g_delta_time     = 0.01;

static U64   g_frame_start_ns = 0;
static float g_frame_time_history[FRAME_TIME_HISTORY_SIZE] = {};
SDL_Window*  g_game_window = nullptr;

ConVar cvar_vsync = {
	.name = "vsync",
	.help = "0 - no vsync, 1 - every vblank, 2 - every 2nd vblank",
	.fvalue = 0,
	.on_change = [](ConVar*) {
		GFX::GraphicsDevice* device = GFX::GraphicsDevice::Get();
		if (device) {
			auto desc = device->GetSwapchainDesc();
			desc.vsync = cvar_vsync.fvalue != 0;
			device->SetSwapchainDesc(desc);
		}
	},
};

void PlatformInitialize() {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		Fatal("SDL_Init: %s", SDL_GetError());
	}

	g_game_window = SDL_CreateWindow("EVA", g_window_size.x, g_window_size.y, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!g_game_window)
	{
		Fatal("SDL_CreateWindow: %s", SDL_GetError());
	}

	ConRegisterVar(&cvar_vsync);
}

void PlatformBeginFrame() {
	{ // Update delta time:

		U64 new_time = SDL_GetTicksNS();
		U64 dt_ns = new_time - g_frame_start_ns;
		g_delta_time = double(dt_ns) / 1'000'000'000;
		g_frame_start_ns = new_time;
	}

	{ // Update window size:
		int w, h;
		SDL_GetWindowSize(g_game_window, &w, &h);
		g_window_size.x = w;
		g_window_size.y = h;
	}

	{ // Update tracked FPS:
		static int k = 0;
		g_frame_time_history[k] = g_delta_time;
		k++;
		if (k >= FRAME_TIME_HISTORY_SIZE) k = 0;

		float avg = 0;
		for (int i = 0; i < EVA_ARRAYSIZE(g_frame_time_history); i++)
		{
			avg += g_frame_time_history[i];
		}
		avg = avg / (float)(FRAME_TIME_HISTORY_SIZE);
		g_fps = 1.0f / avg;
	}
}

bool PlatformProcessSDLEvent(SDL_Event* event) {
	switch (event->type) {
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
			g_quit = true;
			return true;
		}
		default: return false;
	}
}
