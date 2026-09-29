#include <EVA/PAL/PAL.hpp>

namespace EVA::PAL
{

// Compile-only backend; AppKit window and event handling will follow.
void InitBackend()
{
}

void InitWindow(Window* window, const WindowInitOptions&)
{
	window->native_handle = nullptr;
}

void DeinitWindow(Window* window)
{
	window->native_handle = nullptr;
}

void PollBackend()
{
}

}
