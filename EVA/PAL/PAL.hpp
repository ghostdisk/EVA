#pragma once
#include <EVA/Core/Common.hpp>

namespace EVA::PAL
{

struct Window
{
	void* native_handle = nullptr;
};

struct WindowInitOptions
{
	const char* name = "";
	int width = 0;
	int height = 0;
};

enum class EventType
{
	NONE = 0,
	QUIT,
	SURFACE_AVAILABLE,
	SURFACE_UNAVAILABLE,
	WINDOW_RESIZE,
};

struct Event
{
	EventType type = EventType::NONE;
	// WINDOW_RESIZE: new drawable size in pixels.
	int width = 0;
	int height = 0;
};

void Init();
void InitWindow(Window* window, const WindowInitOptions& options);
void DeinitWindow(Window* window);
bool Poll(Event* out_event);


}

namespace EVA
{
int AppMain();
}
