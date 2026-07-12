#include <EVA/App.hpp>
#include <EVA/Core/Basic.hpp>
#include <EVA/Renderer/GraphicsDevice.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <EVA/Net.hpp>
#include <EVA/Assets/Font.hpp>
#include <EVA/Entities/ECamera.hpp>
#include <EVA/Entities/Entity.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Game.hpp>
#include <EVA/GameClient.hpp>
#include <EVA/GameServer.hpp>
#include <EVA/Library.hpp>
#include <EVA/Editor/Editor.hpp>
#include <SDL3/SDL.h>
#include <enet/enet.h>
#include <tracy/Tracy.hpp>
#undef DrawText

// ------------------------------------------------------------

#define SCREEN_LOG_DURATION  1.5f
#define SCREEN_LOG_Y         400
#define SCREEN_LOG_SHIFT_AMT 0.075f

struct ScreenLogEntry {
	char* text;
	float t;
};

struct NextFrameCallback {
	void (*callback)(void* userdata);
	void* userdata;
};

// ------------------------------------------------------------

static std::vector<NextFrameCallback> g_next_frame_callbacks;
static std::vector<ScreenLogEntry>    g_screen_logs;
float  g_screen_log_stagger = 0.0f;

// ------------------------------------------------------------

void ScreenLog(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char* copy = strdup(Vfmt(FrameArena, fmt, args).c_str());
	va_end(args);
	g_screen_log_stagger = 2.0f;

	bool inc = false;
	for (ScreenLogEntry& existing : g_screen_logs) if (existing.t < SCREEN_LOG_SHIFT_AMT) { inc = true; break; }
	if (inc) for (ScreenLogEntry& existing : g_screen_logs) {
		existing.t = roundf(existing.t / SCREEN_LOG_SHIFT_AMT + 1) * SCREEN_LOG_SHIFT_AMT;
	}

	g_screen_logs.push_back(ScreenLogEntry{
		.text = strdup(copy),
		.t = 0.0f,
	});
}

void QueueForNextFrame(void (*callback)(void* userdata), void* userdata) {
	g_next_frame_callbacks.push_back({ callback, userdata });
}

int main() {
	ArenaInitialize();
	RotateFrameArenas();
	PlatformInitialize();
	NetInitialize();
	ConsoleInitialize();
	Result graphicsResult = GFX::GraphicsDevice::Create(GFX::GraphicsDeviceInitDesc{
		.api = GFX::GraphicsAPI::Vulkan,
		.window = g_game_window,
	});
	if (!graphicsResult) Fatal("GraphicsDevice::Create: %s", graphicsResult.error->c_str());
	GameInitialize();
	RendererInitialize();
	FontInitialize();
	AssetsLoad();
	LibraryInitialize();
	InputInitialize();
	UIInitialize();
	EdInitialize();
	GameClientInitialize();
	GameServerInitialize();

	ConExec("game 0");
	ConExec("exec autoexec.cfg");

	while (!g_quit) {
		RotateFrameArenas();
		PlatformBeginFrame();

		InputBeginFrame();
		UIBeginFrame();

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (PlatformProcessSDLEvent(&event)) continue;
			if (UIProcessSDLEvent(&event)) continue;
			if (InputProcessSDLEvent(&event)) continue;
		}
		InputUpdateAxes();

		for (auto& cb : g_next_frame_callbacks) cb.callback(cb.userdata);
		g_next_frame_callbacks.clear();

		Game::TickAll(g_delta_time);

		if (g_active_game) {
			g_active_game->Draw();
		}

		ConsoleDraw();
		UIEndFrame();
		UIDraw();

		// --- screen logs --------------------------------------------
		for (int i = 0; i < g_screen_logs.size(); i++) {
			ScreenLogEntry& entry = g_screen_logs[i];
			float2 size = MeasureText(Library::fnt_arial, entry.text);
			DrawText(Library::fnt_arial, entry.text, -1, g_window_size.x / 2 - size.x/2, SCREEN_LOG_Y - entry.t * 300.0f, float4(1,1,1, 1.0f - entry.t * entry.t));
			g_screen_log_stagger -= 2 * g_delta_time;
			if (g_screen_log_stagger < 0) g_screen_log_stagger = 0;
			if (g_screen_log_stagger < 1) entry.t += (1.0f - g_screen_log_stagger) * g_delta_time / SCREEN_LOG_DURATION;
			if (entry.t > 1) {
				free(entry.text);
				g_screen_logs[i] = g_screen_logs.back();
				g_screen_logs.pop_back();
				i--;
			}
		}

		if (GFX::GraphicsDevice::Get()->BeginFrame()) {
			RenderFrame();
			GFX::GraphicsDevice::Get()->EndFrame();
		}

		FrameMark;
	}

	GFX::GraphicsDevice::Shutdown();
	return 0;
}
