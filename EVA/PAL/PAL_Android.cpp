#include <EVA/PAL/PAL.hpp>
#include <android/looper.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace EVA::PAL
{

static android_app* app = nullptr;

void EmitEvent(Event event);

static void OnAppCmd(android_app*, int32_t command)
{
	if (command == APP_CMD_TERM_WINDOW || command == APP_CMD_DESTROY)
		EmitEvent({ .type = EventType::CLOSE_REQUESTED });
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
	window->native_handle = app->window;
}

void DeinitWindow(Window* window)
{
	window->native_handle = nullptr;
}

void PollBackend()
{
	while (ProcessEvent(0))
	{
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
