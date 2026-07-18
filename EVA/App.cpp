#include <EVA/App.hpp>
#include <EVA/Async/JobSystem.hpp>
#include <EVA/Core/Basic.hpp>
#include <EVA/GFX/GPUDevice.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <EVA/Net.hpp>
#include <EVA/Assets/Font.hpp>
#include <EVA/Entities/ECamera.hpp>
#include <EVA/Entities/Entity.hpp>
#include <EVA/GFX/Renderer.hpp>
#include <EVA/Game.hpp>
#include <EVA/GameClient.hpp>
#include <EVA/GameServer.hpp>
#include <EVA/Library.hpp>
#include <EVA/Editor/Editor.hpp>
#include <SDL3/SDL.h>
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

static Vector<NextFrameCallback> g_next_frame_callbacks;
static Vector<ScreenLogEntry>    g_screen_logs;
float  g_screen_log_stagger = 0.0f;

// ------------------------------------------------------------

void ScreenLog(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char* copy = strdup(g_frameArena->Vfmt(fmt, args).c_str());
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

static void WaitPromise(Promise promise) {
	while (!promise.Done()) {
	}
}

Result RunProgram() {
	ArenaInitialize();
	JobInitialize();
	Arena::RotateFrameArenas();
	PlatformInitialize();
	NetInitialize();
	ConsoleInitialize();

	Promise gpuInitPromise = Promise::Create();

	TRY(GPUDevice::Create({
		.backend       = GPUBackend::Vulkan,
		.window        = g_game_window,
		.signalPromise = gpuInitPromise,
	}));
	
	// WaitPromise(assetsBuildPromise);
	
	GameInitialize();
	FontInitialize();
	RendererInitialize1();
	RendererInitialize2();
	LibraryInitialize();
	InputInitialize();
	UIInitialize();
	EdInitialize();
	GameClientInitialize();
	GameServerInitialize();

	ConExec("game 0");
	ConExec("exec autoexec.cfg");

	while (!g_quit) {
		AssetsScanForChanges();

		Arena::RotateFrameArenas();
		PlatformBeginFrame();

		InputBeginFrame();
		UIBeginFrame();

		GPUDevice::Get()->BeginTransfers();

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

		if (GPUDevice::Get()->BeginFrame()) {
			RenderFrame();
		}
		GPUDevice::Get()->EndFrame();

		FrameMark;
	}

	RendererShutdown();
	GPUDevice::Shutdown();
	JobShutdown();
	return Success();
}

int main() {
	Result res = RunProgram();
	if (res.error)
		Fatal("RunProgram exited with %s", res.error->size);
	return 0;
}
