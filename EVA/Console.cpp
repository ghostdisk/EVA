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
	ConValue (*function)(int num_args, ConValue* args);
	const char* help;
};

static std::vector<char> console_input;
static std::vector<std::string> console_log;
static std::vector<ConCommand> commands;
static std::vector<ConVar*> vars;
static bool console_open = false;

ConValue Con_help(int num_args, ConValue* args)
{
	for (const ConCommand& cmd : commands)
	{
		ConLog("- %s: %s", cmd.name, cmd.help);
	}
	for (const ConVar* var : vars)
	{
		ConLog("- %s: %s", var->name, var->help);
	}
	return {};
}

ConValue Con_clear(int num_args, ConValue* args)
{
	console_log.clear();
	return {};
}

ConValue Con_exec(int num_args, ConValue* args)
{
	char path[256];
	snprintf(path, 256, "%s/%s", EVA_BASE_DIR, "autoexec.cfg");

	char* data = nullptr;
	ReadEntireFile(path, (void**)&data, nullptr);
	if (!data)
	{
		return ConValue{
			.type = ConValueType_Error,
			.string = ArenaPrintf(FrameArena, "failed to open %s", path)
		};
	}
	ConValue val = ConExec(data);
	free(data);
	return val;
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

enum ConTokenType : U8
{
	ConTokenType_EOF = 0,

	ConTokenType_NewLine   = '\n',
	ConTokenType_Semi      = ';',

	ConTokenType_Word   = 128,
	ConTokenType_Number = 129,
	ConTokenType_Error  = 130,
};

struct ConToken
{
	ConTokenType type;
	const char* start;
	const char* end;
	float num;
};


struct ConLexer
{
	const char* start;
	const char* head;
	const char* end;
	ConToken tok;
};

void LexInit(ConLexer& lex, const char* script)
{
	lex.start = script;
	lex.end = script + strlen(script);
	lex.head = script;
	lex.tok = {};
}

static void LexSkipSpace(ConLexer& lex)
{
	while (*lex.head == ' ' || *lex.head == '\t')
	{
		lex.head++;
	}
}

static bool LexIsLetter(char ch)
{
	return ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static bool LexIsDigit(char ch)
{
	return ch >= '0' && ch <= '9';
}

static void ConLexToken(ConLexer& lex)
{
	if (lex.tok.end > lex.head)
	{
		return; // already lexed
	}

	LexSkipSpace(lex);
	lex.tok.start = lex.head;

	switch (*lex.tok.start)
	{
		case '\r':
		{
			lex.tok.end = lex.tok.start + 2;
			lex.tok.type = ConTokenType_NewLine;
			return;
		}
		case ';':
		case '\n':
		{
			lex.tok.end = lex.tok.start + 1;
			lex.tok.type = (ConTokenType)*lex.head;
			return;
		}
	}

	if (LexIsLetter(*lex.head))
	{
		lex.tok.type = ConTokenType_Word;
		lex.tok.end = lex.tok.start;

		while (LexIsLetter(*lex.tok.end) || LexIsDigit(*lex.tok.end))
		{
			lex.tok.end++;
		}
		return;
	}

	if (LexIsDigit(*lex.tok.start) || *lex.tok.start == '.')
	{
		lex.tok.type = ConTokenType_Number;
		lex.tok.end = lex.tok.start;

		while (LexIsLetter(*lex.tok.end) || LexIsDigit(*lex.tok.end) || *lex.tok.end == '.')
		{
			lex.tok.end++;
		}
		lex.tok.num = 0.0f;
		if (lex.tok.end < lex.tok.start + 32)
		{
			char cp[40];
			memcpy(cp, lex.tok.start, lex.tok.end - lex.tok.start);
			cp[lex.tok.end - lex.tok.start] = '\0';
			char* ep = 0;
			lex.tok.num = strtof(cp, &ep);
		}

		return;
	}

	if (*lex.tok.start == '\0')
	{
		lex.tok.type = ConTokenType_EOF;
		lex.tok.end = lex.tok.start;
		return;
	}

	lex.tok.type = ConTokenType_Error;
	lex.tok.end = lex.tok.start;
}

static void ConLexAdvance(ConLexer& lex)
{
	lex.head = lex.tok.end;
}

ConValue ConTokenToValue(const ConToken& tok, Arena* arena)
{
	switch (tok.type)
	{
		case ConTokenType_Error:
			return ConValue{
				.type = ConValueType_Error,
				.string = ArenaInternCString(arena, "parse error"),
			};
		case ConTokenType_Number:
			return ConValue{
				.type = ConValueType_Number,
				.number = tok.num,
			};
		case ConTokenType_Word:
			return ConValue{
				.type = ConValueType_String,
				.string = ArenaInternCString(arena, tok.start, tok.end - tok.start),
			};
		default:
			return ConValue{
				.type = ConValueType_Null,
			};
	}
}

ConValue ConExec1(ConLexer& lex, Arena* arena)
{
	std::vector<ConValue> values;

	const char* expr_start = lex.tok.start;
	const char* expr_end = 0;

	for (;;)
	{
		ConLexToken(lex);
		ConLexAdvance(lex);

		switch (lex.tok.type)
		{
			case ConTokenType_Word:
			{
				values.push_back(ConTokenToValue(lex.tok, arena));
				break;
			}
			case ConTokenType_Error:
			{
				return ConTokenToValue(lex.tok, arena);
			}
			case ConTokenType_NewLine:
			case ConTokenType_Semi:
			case ConTokenType_EOF:
			{
				goto Exec;
			}
			default:
			{
				if (values.size())
				{
					values.push_back(ConTokenToValue(lex.tok, arena));
				}
				else
				{
					expr_end = lex.tok.end;
					ConLog("> %.*s", (int)(expr_end - expr_start), expr_start);
					return ConTokenToValue(lex.tok, arena);
				}
				break;
			}
		}
	}


Exec:
	expr_end = lex.tok.end;
	ConLog("> %.*s", (int)(expr_end - expr_start), expr_start);
	assert(values.size() && values[0].type == ConValueType_String);

	for (ConCommand& cmd : commands)
	{
		if (strcmp(cmd.name, values[0].string) == 0)
		{
			return cmd.function(values.size() - 1, values.data() + 1);
		}
	}
	for (ConVar* var : vars)
	{
		if (strcmp(var->name, values[0].string) == 0)
		{
			if (values.size() == 1)
			{
				return var->value;
			}
			if (var->value.type != values[1].type)
			{
				return ConValue{
					.type = ConValueType_Error,
					.string = ArenaPrintf(arena, "type mismatch"),
				};
			}
			var->value = values[1];
			if (var->on_change) var->on_change(var);

			return var->value;
		}
	}

	return ConValue{
		.type = ConValueType_Error,
		.string = ArenaPrintf(arena, "%s means nothing to me", values[0].string),
	};
}

void ConLog(ConValue val)
{
	switch (val.type)
	{
		case ConValueType_Error:
		{
			ConLog("error: %s", val.string);
			return;
		}
		case ConValueType_String:
		{
			ConLog("\"%s\"", val.string);
			return;
		}
		case ConValueType_Number:
		{
			ConLog("%g", val.number);
			return;
		}
		case ConValueType_Null:
		{
			ConLog("null");
			return;
		}
	}
}

ConValue ConExec(const char* script)
{
	ConLexer lex;
	LexInit(lex, script);

	ConValue val = {};

	for (;;)
	{
		ConLexToken(lex);
		if (lex.tok.type == ConTokenType_EOF)
		{
			break;
		}

		val = ConExec1(lex, FrameArena);
		if (val.type == ConValueType_Error)
		{
			break;
		}
	}

	ConLog(val);
	return val;
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
			->SetSize(800, 600)
			->SetPosition((float)WindowWidth/2 - 400, (float)WindowHeight/2 - 300)
			->SetColor(COLOR_RGB(57, 9, 23));
		DEFER(UIEndBox());

		{ // titlebar
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
			->SetGap(8)
			->SetPadding(8);
		DEFER(UIEndBox());

		{ // main content
			UIBeginBox()
				->SetFlexGrow(1)
				->SetColor(COLOR_BUTTON)
				->SetFlex(UIAxis_Vertical, UIAlignment_Start, UIAlignment_Stretch)
				->SetGap(8)
				->SetPadding(8);

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
				->SetGap(8);

			UIPushId("input");
			text_input = UITextInput(console_input)->SetFlexGrow(1);
			if (just_opened) UIFocus(text_input);
			UIPopId();

			submit = UIButton("Submit");

			UIEndBox();
		}

		if (text_input->flags & UIBoxFlags_Focus) 
		{
			if (InputGetButtonDown(SDL_SCANCODE_RETURN)) submit = true;
			if (InputGetButtonDown(SDL_SCANCODE_L) && InputGetButton(SDL_SCANCODE_LCTRL)) ConExec("clear");
		}

		if (InputGetButtonDown(SDL_SCANCODE_ESCAPE))
		{
			console_open = false;
		}

		if (submit)
		{
			ConExec(ArenaInternCString(FrameArena, console_input.data(), console_input.size()));
			console_input.clear();
		}
	}
}

void ConRegisterCommand(const char* name, ConValue (*function)(int num_args, ConValue* args), const char* help)
{
	commands.push_back(ConCommand{
		.name     = name,
		.function = function,
		.help     = help
	});
}

void ConRegisterVar(ConVar* var)
{
	vars.push_back(var);
}