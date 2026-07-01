#include <EVA/Input.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <SDL3/SDL.h>
#include <EVA/Console.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct InputButtonState
{
	int  button        = 0;
	bool just_pressed  = false;
	bool held          = false;
	bool just_released = false;
};

struct Keybind
{
	int          button;
	const char*  command;
	bool         ctrl;
	bool         shift;
};

struct ButtonNameMapEntry
{
	char name[12];
	int  button;
};

float2 g_mouse_position = {};
float2 g_mouse_delta    = {};

struct Hold
{
	ConVar* var     = nullptr;
	int     button  = 0;
};

static std::vector<InputButtonState>   g_button_states  = {};
static std::vector<Keybind>            g_keybinds       = {};
static std::vector<Hold>               g_holds          = {};

ConVar cvar_forward = { .name = "forward", .fvalue = 0 };
ConVar cvar_right   = { .name = "right",   .fvalue = 0 };
ConVar cvar_left    = { .name = "left",    .fvalue = 0 };
ConVar cvar_back    = { .name = "back",    .fvalue = 0 };
ConVar cvar_flyup   = { .name = "flyup",   .fvalue = 0 };
ConVar cvar_flydown = { .name = "flydown", .fvalue = 0 };

static const ButtonNameMapEntry button_names[] = {
	{ "a",           SDL_SCANCODE_A           },
	{ "b",           SDL_SCANCODE_B           },
	{ "c",           SDL_SCANCODE_C           },
	{ "d",           SDL_SCANCODE_D           },
	{ "e",           SDL_SCANCODE_E           },
	{ "f",           SDL_SCANCODE_F           },
	{ "g",           SDL_SCANCODE_G           },
	{ "h",           SDL_SCANCODE_H           },
	{ "i",           SDL_SCANCODE_I           },
	{ "j",           SDL_SCANCODE_J           },
	{ "k",           SDL_SCANCODE_K           },
	{ "l",           SDL_SCANCODE_L           },
	{ "m",           SDL_SCANCODE_M           },
	{ "n",           SDL_SCANCODE_N           },
	{ "o",           SDL_SCANCODE_O           },
	{ "p",           SDL_SCANCODE_P           },
	{ "q",           SDL_SCANCODE_Q           },
	{ "r",           SDL_SCANCODE_R           },
	{ "s",           SDL_SCANCODE_S           },
	{ "t",           SDL_SCANCODE_T           },
	{ "u",           SDL_SCANCODE_U           },
	{ "v",           SDL_SCANCODE_V           },
	{ "w",           SDL_SCANCODE_W           },
	{ "x",           SDL_SCANCODE_X           },
	{ "y",           SDL_SCANCODE_Y           },
	{ "z",           SDL_SCANCODE_Z           },
	{ "0",           SDL_SCANCODE_0           },
	{ "1",           SDL_SCANCODE_1           },
	{ "2",           SDL_SCANCODE_2           },
	{ "3",           SDL_SCANCODE_3           },
	{ "4",           SDL_SCANCODE_4           },
	{ "5",           SDL_SCANCODE_5           },
	{ "6",           SDL_SCANCODE_6           },
	{ "7",           SDL_SCANCODE_7           },
	{ "8",           SDL_SCANCODE_8           },
	{ "9",           SDL_SCANCODE_9           },
	{ "f1",          SDL_SCANCODE_F1          },
	{ "f2",          SDL_SCANCODE_F2          },
	{ "f3",          SDL_SCANCODE_F3          },
	{ "f4",          SDL_SCANCODE_F4          },
	{ "f5",          SDL_SCANCODE_F5          },
	{ "f6",          SDL_SCANCODE_F6          },
	{ "f7",          SDL_SCANCODE_F7          },
	{ "f8",          SDL_SCANCODE_F8          },
	{ "f9",          SDL_SCANCODE_F9          },
	{ "f9",          SDL_SCANCODE_F9          },
	{ "f9",          SDL_SCANCODE_F9          },
	{ "f10",         SDL_SCANCODE_F10         },
	{ "f11",         SDL_SCANCODE_F11         },
	{ "f12",         SDL_SCANCODE_F12         },
	{ "f13",         SDL_SCANCODE_F13         },
	{ "f14",         SDL_SCANCODE_F14         },
	{ "f15",         SDL_SCANCODE_F15         },
	{ "f16",         SDL_SCANCODE_F16         },
	{ "f17",         SDL_SCANCODE_F17         },
	{ "f18",         SDL_SCANCODE_F18         },
	{ "f19",         SDL_SCANCODE_F19         },
	{ "f20",         SDL_SCANCODE_F20         },
	{ "f21",         SDL_SCANCODE_F21         },
	{ "f22",         SDL_SCANCODE_F22         },
	{ "f23",         SDL_SCANCODE_F23         },
	{ "f24",         SDL_SCANCODE_F24         },
	{ "space",       SDL_SCANCODE_SPACE       },
	{ "tab",         SDL_SCANCODE_TAB         },
	{ "up",          SDL_SCANCODE_UP          },
	{ "down",        SDL_SCANCODE_DOWN        },
	{ "left",        SDL_SCANCODE_LEFT        },
	{ "right",       SDL_SCANCODE_RIGHT       },
	{ "kpplus",      SDL_SCANCODE_KP_PLUS     },
	{ "kpminus",     SDL_SCANCODE_KP_MINUS    },
	{ "equals",      SDL_SCANCODE_EQUALS      },
	{ "minus",       SDL_SCANCODE_MINUS       },
};

void Con_hold(ConParser& parser)
{
	assert(parser.button);

	const char* cvar_name = parser.StringArg();
	if (cvar_name)
	{
		ConVar* cvar = ConGetVar(cvar_name);
		if (cvar)
		{
			cvar->svalue[0] = '1';
			cvar->svalue[1] = '\0';
			cvar->fvalue = 1.0f;
			g_holds.push_back({ cvar, parser.button });
		}
		else
		{
			ConError("hold: %s is not a cvar", cvar_name);
		}
	}
	else
	{
		ConError("hold: missing var name");
	}
}

void Con_bind(ConParser& parser)
{
	const char* button = parser.StringArg();
	const char* cmd = parser.RestArgs();

	bool ctrl = false;
	bool shift = false;
	for (const char* c = button; *c; c++)
	{
		if (*c == '-')
		{
			for (const char* f = button; f < c; f++)
			{
				if (*f == 's') shift = true;
				if (*f == 'c') ctrl = true;
			}
			button = c + 1;
			break;
		}
	}
	for (const ButtonNameMapEntry& entry : button_names)
	{
		if (strcmp(entry.name, button) == 0)
		{
			g_keybinds.push_back({
				.button = entry.button,
				.command = strdup(cmd),
				.ctrl = ctrl,
				.shift = shift,
			});
			return;
		}
	}
	ConError("%s is not a real button", button);
}

void InputInitialize()
{
	ConRegisterCommand("bind", Con_bind, "bind an action to a button");
	ConRegisterCommand("hold", Con_hold, "hold an action (use like this: bind w hold forward)");
	ConRegisterVar(&cvar_forward);
	ConRegisterVar(&cvar_back);
	ConRegisterVar(&cvar_left);
	ConRegisterVar(&cvar_right);
	ConRegisterVar(&cvar_flyup);
	ConRegisterVar(&cvar_flydown);
}

bool TextInputConsumesKey(SDL_Scancode scancode)
{
	if (!SDL_TextInputActive(g_game_window)) return false;
	if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) return true;
	if (scancode >= SDL_SCANCODE_0 && scancode <= SDL_SCANCODE_9) return true;
	return false;
}

static InputButtonState* InputGetButtonState(int button, bool create)
{
	for (InputButtonState& button_state : g_button_states)
	{
		if (button_state.button == button)
		{
			return &button_state;
		}
	}

	if (create)
	{
		g_button_states.push_back(InputButtonState{ .button = button });
		return &g_button_states.back();
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
	g_mouse_position = {};
	g_mouse_delta = {};

	for (int i = 0; i < g_button_states.size(); i++)
	{
		InputButtonState& state = g_button_states[i];
		state.just_pressed = false;
		state.just_released = false;

		if (!state.held)
		{
			state = g_button_states.back();
			g_button_states.pop_back();
			i--;
		}
	}

	float2 old_mouse_position = g_mouse_position;
	SDL_GetMouseState(&g_mouse_position.x, &g_mouse_position.y);
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
			g_mouse_delta.x += event->motion.xrel;
			g_mouse_delta.y += event->motion.yrel;
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
	std::vector<int> consumed_buttons = {};

	auto is_consumed = [&](int key)
		{
			for (int k : consumed_buttons) if (key == k) return true;
			return false;
		};

	auto process = [&](bool ctrl, bool shift)
		{
			for (const Keybind& bind : g_keybinds)
			{
				if (InputGetButtonDown(bind.button))
				{
					int t = 3;
				}
				if (bind.ctrl == ctrl && bind.shift == shift && !is_consumed(bind.button) && (!ctrl||InputGetButton(SDL_SCANCODE_LCTRL)) && (!shift||InputGetButton(SDL_SCANCODE_LSHIFT)))
				{
					if (!TextInputConsumesKey((SDL_Scancode)bind.button) && InputGetButtonDown(bind.button))
					{
						consumed_buttons.push_back(bind.button);
						ConExec(bind.command, bind.button);
					}
				}
			}
		};

	for (int i = 0; i < g_holds.size(); i++)
	{
		if (InputGetButtonUp(g_holds[i].button))
		{
			g_holds[i].var->svalue[0] = '0';
			g_holds[i].var->svalue[1] = '\0';
			g_holds[i].var->fvalue = 0.0f;
			g_holds[i] = g_holds.back();
			g_holds.pop_back();
			i--;
		}
	}

	process(true, true);   // ctrl+shift+*
	process(false, true);  // ctrl+*
	process(true, false);  // shift+*
	process(false, false); // *
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
