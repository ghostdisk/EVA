#include <EVA/PAL/PAL.hpp>
#include <android/looper.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace EVA::PAL
{

static android_app* app = nullptr;
static Window* active_window = nullptr;
static bool resumed = false;

void EmitEvent(Event event);
bool HasPendingEvents();

static void OnAppCmd(android_app* android_app, int32_t command)
{
	switch (command)
	{
	case APP_CMD_INIT_WINDOW:
		if (active_window)
		{
			active_window->native_handle = android_app->window;
			EmitEvent({ .type = EventType::SURFACE_AVAILABLE });
		}
		break;
	case APP_CMD_TERM_WINDOW:
		if (active_window)
		{
			EmitEvent({ .type = EventType::SURFACE_UNAVAILABLE });
			active_window->native_handle = nullptr;
		}
		break;
	case APP_CMD_PAUSE:
		resumed = false;
		break;
	case APP_CMD_RESUME:
		resumed = true;
		break;
	case APP_CMD_DESTROY:
		EmitEvent({ .type = EventType::QUIT });
		break;
	}
}

static bool ProcessEvent(int timeout)
{
	int events = 0;
	android_poll_source* source = nullptr;
	if (ALooper_pollOnce(timeout, nullptr, &events, (void**)&source) < 0)
		return false;
	if (source)
		source->process(app, source);
	return true;
}

void InitBackend()
{
}

void InitWindow(Window* window, const WindowInitOptions&)
{
	active_window = window;
	window->native_handle = app->window;
}

void DeinitWindow(Window* window)
{
	window->native_handle = nullptr;
	active_window = nullptr;
}

void PollBackend()
{
	while (!HasPendingEvents())
	{
		const bool active = resumed && app->window;
		if (!ProcessEvent(active ? 0 : -1) && active)
			break;
	}
}

}

extern "C" void android_main(android_app* app)
{
	EVA::PAL::app = app;
	app->onAppCmd = EVA::PAL::OnAppCmd;
	EVA::PAL::Init();
	while (!app->window && !app->destroyRequested)
		EVA::PAL::ProcessEvent(-1);
	if (!app->destroyRequested)
		EVA::AppMain();
}
