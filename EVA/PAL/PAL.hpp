#pragma once
#include <EVA/OS/OS.hpp>

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
};

struct Event
{
	EventType type = EventType::NONE;
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
