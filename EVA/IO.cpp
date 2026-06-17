#include <EVA/IO.hpp>
#include <EVA/Math.hpp>
#include <SDL3/SDL.h>
#include <vector>

struct IOButtonState
{
	int  button        = 0;
	bool just_pressed  = false;
	bool held          = false;
	bool just_released = false;
};

static std::vector<IOButtonState> ButtonStates = {};

float2 IOMousePosition = {};
float2 IOMouseDelta = {};

void IOInitialize()
{
}

static IOButtonState* IOGetButtonState(int button, bool create)
{
	for (IOButtonState& button_state : ButtonStates)
	{
		if (button_state.button == button)
		{
			return &button_state;
		}
	}

	if (create)
	{
		ButtonStates.push_back(IOButtonState{ .button = button });
		return &ButtonStates.back();
	}
	else
	{
		return nullptr;
	}
}

static void IOPressButton(int button)
{
	IOButtonState* state = IOGetButtonState(button, true);
	state->just_pressed = true;
	state->held = true;
}

static void IOReleaseButton(int button)
{
	IOButtonState* state = IOGetButtonState(button, true);
	state->just_released = true;
	state->held = false;
}

void IOBeginFrame()
{
	for (int i = 0; i < ButtonStates.size(); i++)
	{
		IOButtonState& state = ButtonStates[i];
		state.just_pressed = false;
		state.just_released = false;

		if (!state.held)
		{
			state = ButtonStates.back();
			ButtonStates.pop_back();
			i--;
		}
	}

	float2 old_mouse_position = IOMousePosition;
	SDL_GetMouseState(&IOMousePosition.x, &IOMousePosition.y);
	IOMouseDelta = IOMousePosition - old_mouse_position;
}

bool IOHandleSDLEvent(SDL_Event* event)
{
	switch (event->type)
	{
		case SDL_EVENT_KEY_DOWN:
		{
			IOPressButton(event->key.scancode);
			return true;
		}
		case SDL_EVENT_KEY_UP:
		{
			IOReleaseButton(event->key.scancode);
			return true;
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		{
			IOPressButton(IO_BUTTON_MOUSE_START + event->button.button);
			return true;
		}
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			IOReleaseButton(IO_BUTTON_MOUSE_START + event->button.button);
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool IOGetButtonDown(int button)
{
	IOButtonState* state = IOGetButtonState(button, false);
	return state ? state->just_pressed : false;
}

bool IOGetButton(int button)
{
	IOButtonState* state = IOGetButtonState(button, false);
	return state ? state->held : false;
}

bool IOGetButtonUp(int button)
{
	IOButtonState* state = IOGetButtonState(button, false);
	return state ? state->just_released : false;
}
