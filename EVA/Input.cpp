#include <EVA/Input.hpp>
#include <EVA/Math.hpp>
#include <SDL3/SDL.h>
#include <EVA/Console.hpp>
#include <vector>

struct InputButtonState
{
	int  button        = 0;
	bool just_pressed  = false;
	bool held          = false;
	bool just_released = false;
};

struct InputAxisKeyBinding
{
	InputAxis    axis;
	SDL_Scancode key;
	float        value;
};

struct InputActionKeyBinding
{
	InputAction  action;
	SDL_Scancode key;
};

struct CommandKeyBinding
{
	int          button;
	const char*  command;
};

struct ButtonNameMapEntry {
	char name[12];
	int  button;
};

static std::vector<InputButtonState>      button_states        = {};
static std::vector<InputAxisKeyBinding>   axis_key_bindings    = {};
static std::vector<InputActionKeyBinding> action_key_bindings  = {};
static std::vector<CommandKeyBinding>     command_key_bindings = {};

static const ButtonNameMapEntry button_names[] = {
	{ "a", SDL_SCANCODE_A },
	{ "b", SDL_SCANCODE_B },
	{ "c", SDL_SCANCODE_C },
	{ "d", SDL_SCANCODE_D },
	{ "e", SDL_SCANCODE_E },
	{ "f", SDL_SCANCODE_F },
	{ "g", SDL_SCANCODE_G },
	{ "h", SDL_SCANCODE_H },
	{ "i", SDL_SCANCODE_I },
	{ "j", SDL_SCANCODE_J },
	{ "k", SDL_SCANCODE_K },
	{ "l", SDL_SCANCODE_L },
	{ "m", SDL_SCANCODE_M },
	{ "n", SDL_SCANCODE_N },
	{ "o", SDL_SCANCODE_O },
	{ "p", SDL_SCANCODE_P },
	{ "q", SDL_SCANCODE_Q },
	{ "r", SDL_SCANCODE_R },
	{ "s", SDL_SCANCODE_S },
	{ "t", SDL_SCANCODE_T },
	{ "u", SDL_SCANCODE_U },
	{ "v", SDL_SCANCODE_V },
	{ "w", SDL_SCANCODE_W },
	{ "x", SDL_SCANCODE_X },
	{ "y", SDL_SCANCODE_Y },
	{ "z", SDL_SCANCODE_Z },
	{ "0", SDL_SCANCODE_0 },
	{ "1", SDL_SCANCODE_1 },
	{ "2", SDL_SCANCODE_2 },
	{ "3", SDL_SCANCODE_3 },
	{ "4", SDL_SCANCODE_4 },
	{ "5", SDL_SCANCODE_5 },
	{ "6", SDL_SCANCODE_6 },
	{ "7", SDL_SCANCODE_7 },
	{ "8", SDL_SCANCODE_8 },
	{ "9", SDL_SCANCODE_9 },
	{ "space", SDL_SCANCODE_SPACE },
	{ "tab", SDL_SCANCODE_TAB },
	{ "up", SDL_SCANCODE_UP },
	{ "down", SDL_SCANCODE_DOWN },
	{ "left", SDL_SCANCODE_LEFT },
	{ "right", SDL_SCANCODE_RIGHT },
};

float2 InputMousePosition = {};

static float input_axes[InputAxis_ENUM_SIZE] = {};
static bool input_actions[InputAction_ENUM_SIZE] = {};

void Con_bind(ConParser& parser)
{
	const char* button = parser.StringArg();
	const char* cmd = parser.RestArgs();

	for (const ButtonNameMapEntry& entry : button_names)
	{
		if (strcmp(entry.name, button) == 0)
		{
			command_key_bindings.push_back({ entry.button, strdup(cmd) });
			return;
		}
	}
	ConError("%s is not a real button", button);
}

void InputInitialize()
{
	ConRegisterCommand("bind", Con_bind, "bind an action to a button");
}

bool TextInputConsumesKey(SDL_Scancode scancode)
{
	if (!SDL_TextInputActive(GameWindow)) return false;
	if (scancode >= SDL_SCANCODE_A && SDL_SCANCODE_Z) return true;
	if (scancode >= SDL_SCANCODE_0 && SDL_SCANCODE_9) return true;
	return false;
}

void InputBindKey(InputAxis axis, SDL_Scancode key, float value)
{
	axis_key_bindings.push_back({ .axis = axis, .key = key, .value = value });
}

void InputBindKey(InputAction action, SDL_Scancode key)
{
	action_key_bindings.push_back({ .action = action, .key = key });
}

static InputButtonState* InputGetButtonState(int button, bool create)
{
	for (InputButtonState& button_state : button_states)
	{
		if (button_state.button == button)
		{
			return &button_state;
		}
	}

	if (create)
	{
		button_states.push_back(InputButtonState{ .button = button });
		return &button_states.back();
	}
	else
	{
		return nullptr;
	}
}

static void InputPressButton(int button)
{
	InputButtonState* state = InputGetButtonState(button, true);
	state->just_pressed = true;
	state->held = true;
}

static void InputReleaseButton(int button)
{
	InputButtonState* state = InputGetButtonState(button, true);
	state->just_released = true;
	state->held = false;
}

void InputBeginFrame()
{
	input_axes[InputAxis_MouseX] = 0.0f;
	input_axes[InputAxis_MouseY] = 0.0f;

	for (int i = 0; i < button_states.size(); i++)
	{
		InputButtonState& state = button_states[i];
		state.just_pressed = false;
		state.just_released = false;

		if (!state.held)
		{
			state = button_states.back();
			button_states.pop_back();
			i--;
		}
	}

	float2 old_mouse_position = InputMousePosition;
	SDL_GetMouseState(&InputMousePosition.x, &InputMousePosition.y);

}

bool InputProcessSDLEvent(SDL_Event* event)
{
	switch (event->type)
	{
		case SDL_EVENT_KEY_DOWN:
		{
			InputPressButton(event->key.scancode);
			return true;
		}
		case SDL_EVENT_KEY_UP:
		{
			InputReleaseButton(event->key.scancode);
			return true;
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		{
			InputPressButton(INPUT_BUTTON_MOUSE_START + event->button.button);
			return true;
		}
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			InputReleaseButton(INPUT_BUTTON_MOUSE_START + event->button.button);
			return true;
		}
		case SDL_EVENT_MOUSE_MOTION:
		{
			input_axes[InputAxis_MouseX] += event->motion.xrel;
			input_axes[InputAxis_MouseY] += event->motion.yrel;
			return true;
		}
		default:
		{
			return false;
		}
	}
}

void InputUpdateAxes()
{
	for (InputAxisKeyBinding& binding : axis_key_bindings)
	{
		input_axes[binding.axis] = 0.0f;
	}

	for (int i = 0; i < InputAction_ENUM_SIZE; i++)
	{
		input_actions[i] = false;
	}

	for (InputAxisKeyBinding& binding : axis_key_bindings)
	{
		InputButtonState* button_state = InputGetButtonState(binding.key, false);
		if (button_state)
		{
			if (button_state->held && !TextInputConsumesKey(binding.key))  input_axes[binding.axis] += binding.value;
		}
	}

	for (InputActionKeyBinding& binding : action_key_bindings)
	{
		if (InputGetButtonDown(binding.key) && !TextInputConsumesKey(binding.key))
		{
			input_actions[binding.action] = true;
		}
	}

	for (const CommandKeyBinding& bind : command_key_bindings)
	{
		if (!TextInputConsumesKey((SDL_Scancode)bind.button) && InputGetButtonDown(bind.button))
		{
			ConExec(bind.command);
		}
	}
}

bool InputGetButtonDown(int button)
{
	InputButtonState* state = InputGetButtonState(button, false);
	return state ? state->just_pressed : false;
}

bool InputGetButton(int button)
{
	InputButtonState* state = InputGetButtonState(button, false);
	return state ? state->held : false;
}

bool InputGetButtonUp(int button)
{
	InputButtonState* state = InputGetButtonState(button, false);
	return state ? state->just_released : false;
}

float InputGetAxis(InputAxis axis)
{
	return input_axes[axis];
}