#pragma once
#include <SDL3/SDL_scancode.h>

struct float2;
union SDL_Event;

enum InputAxis
{
	InputAxis_None = 0,
	InputAxis_Horizontal,
	InputAxis_Vertical,
	InputAxis_Fly,
	InputAxis_MouseX,
	InputAxis_MouseY,

	InputAxis_ENUM_SIZE,
};

enum InputAction
{
	InputAction_None = 0,
	InputAction_Console,

	InputAction_ENUM_SIZE,
};

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
float InputGetAxis(InputAxis axis);

void InputBindKey(InputAxis axis, SDL_Scancode key, float value);
void InputBindKey(InputAction action, SDL_Scancode key);

extern float2 InputMousePosition;