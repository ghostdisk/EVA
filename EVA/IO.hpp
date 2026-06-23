#pragma once
#include <SDL3/SDL_scancode.h>

struct float2;
union SDL_Event;

#define IO_BUTTON_MOUSE_START  1'000'000
#define IO_BUTTON_MOUSE_LEFT   1'000'001
#define IO_BUTTON_MOUSE_MIDDLE 1'000'002
#define IO_BUTTON_MOUSE_RIGHT  1'000'003
#define IO_BUTTON_MOUSE_4      1'000'004
#define IO_BUTTON_MOUSE_5      1'000'005

void IOInitialize();
void IOBeginFrame();
bool IOProcessSDLEvent(SDL_Event* event);


// Either call with IO_BUTTON_MOUSE_* or with SDL_SCANCODE_*
bool IOGetButtonDown(int button);
bool IOGetButton(int button);
bool IOGetButtonUp(int button);

extern float2 IOMousePosition;
extern float2 IOMouseDelta;