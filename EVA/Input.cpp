#include <EVA/Input.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <SDL3/SDL.h>
#include <EVA/Console.hpp>
#include <EVA/Math.hpp>

struct InputButtonState {
	int  button        = 0;
	bool just_pressed  = false;
	bool held          = false;
	bool just_released = false;
};

struct Keybind {
	int          button;
	const char*  command;
	bool         ctrl;
	bool         shift;
};

struct ButtonNameMapEntry {
	char name[12];
	int  button;
};

struct Hold {
	ConVar* var     = nullptr;
	int     button  = 0;
};


float2 g_mouse_position = {};
float2 g_mouse_delta    = {};

static Vector<InputButtonState>   g_button_states  = {};
static Vector<Keybind>            g_keybinds       = {};
static Vector<Hold>               g_holds          = {};

ConVar cvar_forward = { .name = "forward", .fvalue = 0 };
ConVar cvar_right   = { .name = "right",   .fvalue = 0 };
ConVar cvar_left    = { .name = "left",    .fvalue = 0 };
ConVar cvar_back    = { .name = "back",    .fvalue = 0 };
ConVar cvar_flyup   = { .name = "flyup",   .fvalue = 0 };
ConVar cvar_flydown = { .name = "flydown", .fvalue = 0 };

static const ButtonNameMapEntry button_names[] = {
	{ "a",           SCANCODE_A           },
	{ "b",           SCANCODE_B           },
	{ "c",           SCANCODE_C           },
	{ "d",           SCANCODE_D           },
	{ "e",           SCANCODE_E           },
	{ "f",           SCANCODE_F           },
	{ "g",           SCANCODE_G           },
	{ "h",           SCANCODE_H           },
	{ "i",           SCANCODE_I           },
	{ "j",           SCANCODE_J           },
	{ "k",           SCANCODE_K           },
	{ "l",           SCANCODE_L           },
	{ "m",           SCANCODE_M           },
	{ "n",           SCANCODE_N           },
	{ "o",           SCANCODE_O           },
	{ "p",           SCANCODE_P           },
	{ "q",           SCANCODE_Q           },
	{ "r",           SCANCODE_R           },
	{ "s",           SCANCODE_S           },
	{ "t",           SCANCODE_T           },
	{ "u",           SCANCODE_U           },
	{ "v",           SCANCODE_V           },
	{ "w",           SCANCODE_W           },
	{ "x",           SCANCODE_X           },
	{ "y",           SCANCODE_Y           },
	{ "z",           SCANCODE_Z           },
	{ "0",           SCANCODE_0           },
	{ "1",           SCANCODE_1           },
	{ "2",           SCANCODE_2           },
	{ "3",           SCANCODE_3           },
	{ "4",           SCANCODE_4           },
	{ "5",           SCANCODE_5           },
	{ "6",           SCANCODE_6           },
	{ "7",           SCANCODE_7           },
	{ "8",           SCANCODE_8           },
	{ "9",           SCANCODE_9           },
	{ "f1",          SCANCODE_F1          },
	{ "f2",          SCANCODE_F2          },
	{ "f3",          SCANCODE_F3          },
	{ "f4",          SCANCODE_F4          },
	{ "f5",          SCANCODE_F5          },
	{ "f6",          SCANCODE_F6          },
	{ "f7",          SCANCODE_F7          },
	{ "f8",          SCANCODE_F8          },
	{ "f9",          SCANCODE_F9          },
	{ "f9",          SCANCODE_F9          },
	{ "f9",          SCANCODE_F9          },
	{ "f10",         SCANCODE_F10         },
	{ "f11",         SCANCODE_F11         },
	{ "f12",         SCANCODE_F12         },
	{ "f13",         SCANCODE_F13         },
	{ "f14",         SCANCODE_F14         },
	{ "f15",         SCANCODE_F15         },
	{ "f16",         SCANCODE_F16         },
	{ "f17",         SCANCODE_F17         },
	{ "f18",         SCANCODE_F18         },
	{ "f19",         SCANCODE_F19         },
	{ "f20",         SCANCODE_F20         },
	{ "f21",         SCANCODE_F21         },
	{ "f22",         SCANCODE_F22         },
	{ "f23",         SCANCODE_F23         },
	{ "f24",         SCANCODE_F24         },
	{ "space",       SCANCODE_SPACE       },
	{ "tab",         SCANCODE_TAB         },
	{ "up",          SCANCODE_UP          },
	{ "down",        SCANCODE_DOWN        },
	{ "left",        SCANCODE_LEFT        },
	{ "right",       SCANCODE_RIGHT       },
	{ "kpplus",      SCANCODE_KP_PLUS     },
	{ "kpminus",     SCANCODE_KP_MINUS    },
	{ "equals",      SCANCODE_EQUALS      },
	{ "minus",       SCANCODE_MINUS       },
};

Result Con_hold(ConParser& parser) {
	ScratchArena scratch;
	if (!parser.button) return Err("hold can only be called from a response to bind (e.g. bind w hold forward)");

	ZTString cvar_name = parser.StringArg(scratch);
	if (cvar_name) {
		ConVar* cvar = ConGetVar(cvar_name);
		if (cvar) {
			cvar->svalue[0] = '1';
			cvar->svalue[1] = '\0';
			cvar->fvalue = 1.0f;
			g_holds.push_back({ cvar, parser.button });
		} else {
			return Err("hold: %s is not a cvar", cvar_name);
		}
	} else {
		return Err("hold: missing var name");
	}
	return Success();
}

Result Con_bind(ConParser& parser) {
	ScratchArena scratch;

	ZTString button = parser.StringArg(scratch);
	ZTString cmd = parser.RestArgs(scratch);

	bool ctrl = false;
	bool shift = false;
	for (const char* c = button.c_str(); *c; c++) {
		if (*c == '-') {
			for (const char* f = button.c_str(); f < c; f++) {
				if (*f == 's') shift = true;
				if (*f == 'c') ctrl = true;
			}
			button = c + 1;
			break;
		}
	}
	for (const ButtonNameMapEntry& entry : button_names) {
		if (strcmp(entry.name, button) == 0) {
			g_keybinds.push_back({
				.button = entry.button,
				.command = cmd.CopyToHeap(),
				.ctrl = ctrl,
				.shift = shift,
			});
			return Success();
		}
	}
	return Err("%s is not a real button", button);
}

void InputInitialize() {
	ConRegisterCommand("bind", Con_bind, "bind an action to a button");
	ConRegisterCommand("hold", Con_hold, "hold an action (use like this: bind w hold forward)");
	ConRegisterVar(&cvar_forward);
	ConRegisterVar(&cvar_back);
	ConRegisterVar(&cvar_left);
	ConRegisterVar(&cvar_right);
	ConRegisterVar(&cvar_flyup);
	ConRegisterVar(&cvar_flydown);
}

bool TextInputConsumesKey(SDL_Scancode scancode) {
	if (!SDL_TextInputActive(g_game_window)) return false;
	if (scancode >= SCANCODE_A && scancode <= SCANCODE_Z) return true;
	if (scancode >= SCANCODE_1 && scancode <= SCANCODE_9) return true;
	if (scancode >= SCANCODE_0) return true; // zero is non-sequential here
	if (scancode == SCANCODE_MINUS || scancode == SCANCODE_EQUALS) return true;
	return false;
}

static InputButtonState* InputGetButtonState(int button, bool create) {
	for (InputButtonState& button_state : g_button_states) {
		if (button_state.button == button) {
			return &button_state;
		}
	}

	if (create) {
		g_button_states.push_back(InputButtonState{ .button = button });
		return &g_button_states.back();
	} else {
		return nullptr;
	}
}

static void InputPressButton(int button) {
	InputButtonState* state = InputGetButtonState(button, true);
	state->just_pressed = true;
	state->held = true;
}

static void InputReleaseButton(int button) {
	InputButtonState* state = InputGetButtonState(button, true);
	state->just_released = true;
	state->held = false;
}

void InputBeginFrame() {
	g_mouse_position = {};
	g_mouse_delta = {};

	for (int i = 0; i < g_button_states.size(); i++) {
		InputButtonState& state = g_button_states[i];
		state.just_pressed = false;
		state.just_released = false;

		if (!state.held) {
			state = g_button_states.back();
			g_button_states.pop_back();
			i--;
		}
	}

	float2 old_mouse_position = g_mouse_position;
	SDL_GetMouseState(&g_mouse_position.x, &g_mouse_position.y);
}

bool InputProcessSDLEvent(SDL_Event* event) {
	switch (event->type) {
		case SDL_EVENT_KEY_DOWN: {
			InputPressButton(event->key.scancode);
			return true;
		}
		case SDL_EVENT_KEY_UP: {
			InputReleaseButton(event->key.scancode);
			return true;
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN: {
			InputPressButton(INPUT_BUTTON_MOUSE_START + event->button.button);
			return true;
		}
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			InputReleaseButton(INPUT_BUTTON_MOUSE_START + event->button.button);
			return true;
		}
		case SDL_EVENT_MOUSE_MOTION: {
			g_mouse_delta.x += event->motion.xrel;
			g_mouse_delta.y += event->motion.yrel;
			return true;
		}
		default: return false; 
	}
}

void InputUpdateAxes() {
	Vector<int> consumed_buttons = {};

	auto is_consumed = [&](int key) {
		for (int k : consumed_buttons) if (key == k) return true;
		return false;
	};

	auto process = [&](bool ctrl, bool shift) {
		for (const Keybind& bind : g_keybinds)
		{
			if (bind.ctrl != ctrl)                               continue;
			if (bind.shift != shift)                             continue;
			if (is_consumed(bind.button))                        continue;
			if (ctrl && !InputGetButton(SCANCODE_LCTRL))     continue;
			if (shift && !InputGetButton(SCANCODE_LSHIFT))   continue;
			if (!InputGetButtonDown(bind.button))                continue;
			if (TextInputConsumesKey((SDL_Scancode)bind.button)) continue;

			consumed_buttons.push_back(bind.button);
			ConExec(bind.command, bind.button);
		}
	};

	for (int i = 0; i < g_holds.size(); i++) {
		if (InputGetButtonUp(g_holds[i].button)) {
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

bool InputGetButtonDown(int button) {
	InputButtonState* state = InputGetButtonState(button, false);
	return state ? state->just_pressed : false;
}

bool InputGetButton(int button) {
	InputButtonState* state = InputGetButtonState(button, false);
	return state ? state->held : false;
}

bool InputGetButtonUp(int button) {
	InputButtonState* state = InputGetButtonState(button, false);
	return state ? state->just_released : false;
}
