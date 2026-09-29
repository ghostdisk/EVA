#include <EVA/PAL/PAL.hpp>
#include <Windows.h>
#include <cstring>

namespace EVA::PAL
{

static constexpr char WINDOW_CLASS_NAME[] = "EVA_PAL_Window";

static HINSTANCE hinstance = nullptr;

void EmitEvent(Event event);

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (message == WM_CLOSE)
	{
		EmitEvent({ .type = EventType::QUIT });
		return 0;
	}
	return DefWindowProcA(window, message, wparam, lparam);
}

void InitBackend()
{
	hinstance = GetModuleHandleA(0);

	WNDCLASSEXA window_class = {};
	window_class.cbSize = sizeof(window_class);
	window_class.lpfnWndProc = WindowProc;
	window_class.hInstance = hinstance;
	window_class.hCursor = LoadCursorA(nullptr, IDC_ARROW);
	window_class.lpszClassName = WINDOW_CLASS_NAME;
	window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	RegisterClassExA(&window_class);
}

void InitWindow(Window* window, const WindowInitOptions& options)
{
	const char* name = options.name;

	HWND handle = CreateWindowExA(0, WINDOW_CLASS_NAME, name, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		options.width > 0 ? options.width : CW_USEDEFAULT,
		options.height > 0 ? options.height : CW_USEDEFAULT,
		nullptr, nullptr, hinstance, nullptr);

	window->native_handle = handle;
	if (handle) 
	{
		ShowWindow(handle, SW_SHOW);
	}
}

void DeinitWindow(Window* window)
{
	if (window->native_handle)
		DestroyWindow(static_cast<HWND>(window->native_handle));
	memset(window, 0, sizeof(*window));
}

void PollBackend()
{
	MSG message = {};
	while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE))
	{
		if (message.message == WM_QUIT)
		{
			EmitEvent({ .type = EventType::QUIT });
			continue;
		}
		TranslateMessage(&message);
		DispatchMessageA(&message);
	}
}

}
