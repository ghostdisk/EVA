#include <EVA/Core/Arena.hpp>
#include <EVA/Core/String.hpp>
#include <EVA/Console.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <EVA/Platform.hpp>
#include <string>
#include <vector>
#include <stdarg.h>

struct ConCommand {
	const char* name;
	ConProc proc;
	const char* help;
};

static std::vector<char>        g_console_input   = {};
static std::vector<std::string> g_console_log     = {};
static std::vector<ConCommand>  g_commands        = {};
static std::vector<ConVar*>     g_cvars           = {};
static bool                     console_open      = false;

Result Con_help(ConParser& parser) {
	for (const ConCommand& cmd : g_commands)
		ConLog("- %s: %s", cmd.name, cmd.help);
	for (const ConVar* var : g_cvars) 
		ConLog("- %s: %s", var->name, var->help);
	return Success();
}

Result Con_clear(ConParser& parser) {
	g_console_log.clear();
	return Success();
}

Result Con_exec(ConParser& parser) {
	char path[256];
	snprintf(path, 256, "%s/%s", EVA_BASE_DIR, parser.StringArg());

	char* data = nullptr;
	ReadEntireFile(path, (void**)&data, nullptr);
	if (!data) {
		return Err("failed to open %s", path);
	}
	ConExec(data);
	free(data);

	return Success();
}

void ConsoleInitialize() {
	ConRegisterCommand("clear", Con_clear, "clear the console");
	ConRegisterCommand("help", Con_help, "list commands");
	ConRegisterCommand("exec", Con_exec, "execute a script file");
}

ConVar* ConGetVar(const char* name) {
	for (ConVar* cvar : g_cvars)
		if (strcmp(cvar->name, name) == 0)
			return cvar;
	return nullptr;
}

void ConLog(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ZTString str = Vfmt(FrameArena, fmt, args);
	va_end(args);
	g_console_log.push_back(str.c_str());
}

void ConError(Result err) {
	assert(!err);
	g_console_log.push_back(Fmt(FrameArena, "error: %s", err.error->c_str()).c_str());
}

static bool ConSkipSpace(ConParser& parser) {
	if (*parser.head != ' ' && *parser.head != '\t') return false;
	while (*parser.head == ' ' || *parser.head == '\t') parser.head++;
	return true;
}

static void ConSkipLine(ConParser& parser) {
	while (*parser.head != '\n' && *parser.head != '\0') parser.head++;
	if (*parser.head == '\n') parser.head++;
}

static bool ConSkipComment(ConParser& parser) {
	if (*parser.head != '#') return false;
	while (*parser.head != '\n' && *parser.head != '\0') parser.head++;
	return true;
}

const char* ConParser::StringArg() {
	ConSkipSpace(*this);
	if (ConSkipComment(*this)) return nullptr;

	if (*head == '\n' || *head == '\r' || *head == '\r') return nullptr;

	const char* start = head;
	while ((*head > ' ' && *head <= '~')) {
		head++;
	}
	return *start ? ArenaInternCString(FrameArena, start, head - start) : nullptr;
}

const char* ConParser::RestArgs() {
	ConSkipSpace(*this);

	const char* start = head;
	while (*head != '#' && *head != '\n' && *head != '\r' && *head != '\0') {
		head++;
	}
	return ArenaInternCString(FrameArena, start, head - start);
}

float ConParser::FloatArg(float fallback) {
	const char* s = StringArg();
	if (!s) return fallback;

	char* endptr = 0;
	float f = strtof(s, &endptr);
	if (*endptr == '\0' && endptr > s) return f;
	else return fallback;
}

int ConParser::IntArg(int fallback) {
	const char* s = StringArg();
	if (!s) return fallback;

	char* endptr = 0;
	int f = (int)strtol(s, &endptr, 10);
	if (*endptr == '\0' && endptr > s) return f;
	else return fallback;
}

Result ConExec(const char* script, int button) {
	ConParser parser;
	parser.start = script;
	parser.end = script + strlen(script);
	parser.head = script;
	parser.button = button;
	return ConExec(parser);
}

Result ConExecSingle(ConParser& parser) {
	const char* cmd = parser.StringArg();
	if (!cmd) {
		ConSkipLine(parser);
		return Success();
	}

	for (const ConCommand& c : g_commands) {
		if (strcmp(c.name, cmd) == 0) {
			Result res = c.proc(parser);
			ConSkipLine(parser);
			return res;
		}
	}

	ConVar* cvar = ConGetVar(cmd);
	if (cvar) {
		const char* value = parser.StringArg();
		if (value) {
			snprintf(cvar->svalue, sizeof(cvar->svalue), "%s", value);
			char* endptr = 0;
			float f = strtof(cvar->svalue, &endptr);
			if (*endptr == '\0') {
				cvar->fvalue = f;
			}
			ConSkipLine(parser);
			if (cvar->on_change) cvar->on_change(cvar);
			return Success();
		} else {
			ConLog("%s is '%s'", cvar->name, cvar->svalue);
			return Success();
		}
	}

	ConSkipLine(parser);
	return Err("%s means nothing to me", cmd);
}

Result ConExec(ConParser& parser) {
	Result res = Success();

	while (parser.head < parser.end) {
		Result r = ConExecSingle(parser);
		if (!r) {
			res = r;
			ConError(r);
		}
	}

	return res;
}

void ConsoleDraw() {
	bool just_opened = false;

	if (InputGetButtonDown(SDL_SCANCODE_GRAVE)) {
		console_open = !console_open;
		just_opened = true;
		g_console_input.clear();
	}

	if (console_open) {
		UIPushId("console");
		DEFER(UIPopId());

		UIBeginBox()
			->SetFlex(UIAxis_Vertical, UIAlignment_Stretch, UIAlignment_Stretch)
			->SetSize(g_window_size.x, 400)
			->SetColor(COLOR_RGB(57, 9, 23));
		DEFER(UIEndBox());

		if (0) { // titlebar
			UIBeginBox()
				->SetFlex(UIAxis_Horizontal, UIAlignment_Stretch, UIAlignment_Stretch);
			UILabel("Console")->SetPadding(8);

			UIBeginBox()->SetFlexGrow(1);
			UIEndBox(); // spacer

			if (UIButton("X")) console_open = false;
			UIEndBox();
		}

		// window padding box
		UIBeginBox()
			->SetFlex(UIAxis_Vertical, UIAlignment_Stretch, UIAlignment_Stretch)
			->SetFlexGrow(1)
			->SetGap(4)
			->SetPadding(4);
		DEFER(UIEndBox());

		{ // main content
			UIBeginBox()
				->SetFlexGrow(1)
				->SetColor(COLOR_BUTTON)
				->SetFlex(UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch)
				->SetGap(4)
				->SetPadding(4);

			for (const std::string& msg : g_console_log) {
				UILabel(msg.c_str());
			}

			UIEndBox();
		}

		bool submit = false;
		UIBox* text_input;

		{ // input bar
			UIBeginBox()
				->SetFlex(UIAxis_Horizontal, UIAlignment_Stretch, UIAlignment_Stretch)
				->SetGap(4);

			UIPushId("input");
			text_input = UITextInput(g_console_input)->SetFlexGrow(1);
			if (just_opened) UIFocus(text_input);
			UIPopId();

			submit = UIButton("Submit");

			UIEndBox();
		}

		if (text_input->flags & UIBoxFlags_Focus) {
			if (InputGetButtonDown(SDL_SCANCODE_RETURN) || InputGetButtonDown(SDL_SCANCODE_KP_ENTER)) submit = true;
			if (InputGetButtonDown(SDL_SCANCODE_L) && InputGetButton(SDL_SCANCODE_LCTRL)) ConExec("clear");
		}

		if (InputGetButtonDown(SDL_SCANCODE_ESCAPE)) console_open = false;
		if (submit) {
			ConExec(ArenaInternCString(FrameArena, g_console_input.data(), g_console_input.size()));
			g_console_input.clear();
		}
	}
}

void ConRegisterCommand(const char* name, ConProc proc, const char* help) {
	g_commands.push_back(ConCommand{
		.name     = name,
		.proc     = proc,
		.help     = help
	});
}

void ConRegisterVar(ConVar* var) {
	g_cvars.push_back(var);
}