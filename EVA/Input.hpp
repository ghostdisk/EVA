#pragma once
#include <SDL3/SDL_scancode.h>

struct float2;
struct ConVar;
union SDL_Event;

#define INPUT_BUTTON_MOUSE_START  1'000'000
#define INPUT_BUTTON_MOUSE_LEFT   1'000'001
#define INPUT_BUTTON_MOUSE_MIDDLE 1'000'002
#define INPUT_BUTTON_MOUSE_RIGHT  1'000'003
#define INPUT_BUTTON_MOUSE_4      1'000'004
#define INPUT_BUTTON_MOUSE_5      1'000'005

void InputInitialize();
void InputBeginFrame();
void InputUpdateAxes();
bool InputProcessSDLEvent(SDL_Event* event);

// Either call with INPUT_BUTTON_MOUSE_* or with SDL_SCANCODE_*
bool InputGetButtonDown(int button);
bool InputGetButton(int button);
bool InputGetButtonUp(int button);

extern float2 g_mouse_position;
extern float2 g_mouse_delta;

extern ConVar cvar_forward;
extern ConVar cvar_right;
extern ConVar cvar_left;
extern ConVar cvar_back;
extern ConVar cvar_flyup;
extern ConVar cvar_flydown;