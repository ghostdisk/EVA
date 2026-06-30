#include <EVA/Arena.hpp>
#include <EVA/Console.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <EVA/Platform.hpp>
#include <string>
#include <vector>
#include <stdarg.h>

struct ConCommand
{
	const char* name;
	ConProc proc;
	const char* help;
};

static std::vector<char> console_input;
static std::vector<std::string> console_log;
static std::vector<ConCommand> commands;
static std::vector<ConVar*> vars;
static bool console_open = false;

void Con_help(ConParser& parser)
{
	for (const ConCommand& cmd : commands)
	{
		ConLog("- %s: %s", cmd.name, cmd.help);
	}
	for (const ConVar* var : vars)
	{
		ConLog("- %s: %s", var->name, var->help);
	}
}

void Con_clear(ConParser& parser)
{
	console_log.clear();
}

void Con_exec(ConParser& parser)
{
	char path[256];
	snprintf(path, 256, "%s/%s", EVA_BASE_DIR, parser.StringArg());

	char* data = nullptr;
	ReadEntireFile(path, (void**)&data, nullptr);
	if (!data)
	{
		return ConError("failed to open %s", path);
	}
	ConExec(data);
	free(data);
}

void ConsoleInitialize()
{
	ConRegisterCommand("clear", Con_clear, "clear the console");
	ConRegisterCommand("help", Con_help, "list commands");
	ConRegisterCommand("exec", Con_exec, "execute a script file");
}

void ConLog(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const char* str = ArenaVprintf(FrameArena, fmt, args);
	va_end(args);
	console_log.push_back(str);
}

void ConError(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const char* str = ArenaVprintf(FrameArena, fmt, args);
	va_end(args);
	console_log.push_back(ArenaPrintf(FrameArena, "error: %s", str));
}

static bool ConSkipSpace(ConParser& parser)
{
	if (*parser.head != ' ' && *parser.head != '\t') return false;
	while (*parser.head == ' ' || *parser.head == '\t') parser.head++;
	return true;
}

static void ConSkipLine(ConParser& parser)
{
	while (*parser.head != '\n' && *parser.head != '\0') parser.head++;
	if (*parser.head == '\n') parser.head++;
}

static bool ConSkipComment(ConParser& parser)
{
	if (*parser.head != '#') return false;
	while (*parser.head != '\n' && *parser.head != '\0') parser.head++;
	return true;
}

const char* ConParser::StringArg()
{
	ConSkipSpace(*this);
	if (ConSkipComment(*this)) return nullptr;

	if (*head == '\n' || *head == '\r' || *head == '\r') return nullptr;

	const char* start = head;
	while ((*head > ' ' && *head <= '~'))
	{
		head++;
	}
	return *start ? ArenaInternCString(FrameArena, start, head - start) : nullptr;
}

const char* ConParser::RestArgs()
{
	ConSkipSpace(*this);

	const char* start = head;
	while (*head != '#' && *head != '\n' && *head != '\r' && *head != '\0')
	{
		head++;
	}
	return ArenaInternCString(FrameArena, start, head - start);
}

float ConParser::FloatArg(float fallback)
{
	const char* s = StringArg();
	if (!s) return fallback;

	char* endptr = 0;
	float f = strtof(s, &endptr);
	if (*endptr == '\0' && endptr > s) return f;
	else return fallback;
}

void ConExec(const char* script)
{
	ConParser parser;
	parser.start = script;
	parser.end = script + strlen(script);
	parser.head = script;
	ConExec(parser);
}

bool ConExecSingle(ConParser& parser)
{
	const char* cmd = parser.StringArg();
	if (!cmd)
	{
		ConSkipLine(parser);
		return false;
	}

	for (ConCommand c : commands)
	{
		if (strcmp(c.name, cmd) == 0)
		{
			c.proc(parser);
			return true;
		}
	}

	const char* value = parser.StringArg();
	if (value)
	{
		for (ConVar* var : vars)
		{
			if (strcmp(var->name, cmd) == 0)
			{
				snprintf(var->svalue, sizeof(var->svalue), "%s", value);
				char* endptr = 0;
				float f = strtof(var->svalue, &endptr);
				if (*endptr == '\0')
				{
					var->fvalue = f;
				}
				ConSkipLine(parser);
				if (var->on_change) var->on_change(var);
				return true;
			}
		}
	}

	ConError("%s means nothing to me", cmd);
	ConSkipLine(parser);
	return false;
}

void ConExec(ConParser& parser)
{
	while (parser.head < parser.end)
	{
		ConExecSingle(parser);
	}
}

void ConsoleDraw()
{
	bool just_opened = false;

	if (InputGetButtonDown(SDL_SCANCODE_GRAVE))
	{
		console_open = !console_open;
		just_opened = true;
		console_input.clear();
	}

	if (console_open)
	{
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

			for (const std::string& msg : console_log)
			{
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
			text_input = UITextInput(console_input)->SetFlexGrow(1);
			if (just_opened) UIFocus(text_input);
			UIPopId();

			submit = UIButton("Submit");

			UIEndBox();
		}

		if (text_input->flags & UIBoxFlags_Focus) 
		{
			if (InputGetButtonDown(SDL_SCANCODE_RETURN) || InputGetButtonDown(SDL_SCANCODE_KP_ENTER)) submit = true;
			if (InputGetButtonDown(SDL_SCANCODE_L) && InputGetButton(SDL_SCANCODE_LCTRL)) ConExec("clear");
		}

		if (InputGetButtonDown(SDL_SCANCODE_ESCAPE)) console_open = false;
		if (submit)
		{
			ConExec(ArenaInternCString(FrameArena, console_input.data(), console_input.size()));
			console_input.clear();
		}
	}
}

void ConRegisterCommand(const char* name, ConProc proc, const char* help)
{
	commands.push_back(ConCommand{
		.name     = name,
		.proc     = proc,
		.help     = help
	});
}

void ConRegisterVar(ConVar* var)
{
	vars.push_back(var);
}