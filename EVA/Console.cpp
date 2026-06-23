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
	int (*function)(int argc, const char** argv);
	const char* help;
};

std::vector<char> console_input;
std::vector<std::string> console_log;
std::vector<ConCommand> commands;
static bool console_open = false;

int Con_help(int argc, const char** argv)
{
	for (const ConCommand& cmd : commands)
	{
		ConLog("- %s: %s", cmd.name, cmd.help);
	}
	return 0;
}

int Con_clear(int argc, const char** argv)
{
	console_log.clear();
	return 0;
}

void ConsoleInitialize()
{
	ConRegisterCommand("clear", Con_clear, "clear the console");
	ConRegisterCommand("help", Con_help, "list commands");
}

void ConLog(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const char* str = ArenaVprintf(FrameArena, fmt, args);
	va_end(args);
	console_log.push_back(str);
}

int ConExec(const char* cmd)
{
	std::vector<const char*> argv;
	char* cmd_copy = ArenaInternCString(FrameArena, cmd);

	char* word_start = cmd_copy;

	for (char* c = cmd_copy;; c++)
	{
		if (isspace(*c) || *c == '\0')
		{
			bool exit = *c == '\0';
			*c = '\0';
			if (word_start == c)
			{
				word_start++;
			}
			if (word_start && word_start < c)
			{
				argv.push_back(word_start);
				word_start = nullptr;
			}
			if (exit) break;
		}
		else if (!word_start) word_start = c;
	}

	if (argv.size() == 0)
	{
		return -1;
	}
	ConLog("> %s", cmd);

	for (const auto& command : commands)
	{
		if (strcmp(command.name, argv[0]) == 0)
		{
			return command.function(argv.size(), argv.data());
		}
	}

	ConLog("%s means nothing to me", argv[0]);
	return -1;
}

void ConsoleDraw()
{
	bool just_opened = false;

	if (!console_open && InputGetButtonDown(SDL_SCANCODE_GRAVE))
	{
		console_open = true;
		just_opened = true;
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

		UIBeginBox()
			->SetFlex(UIAxis_Vertical, UIAlignment_Stretch, UIAlignment_Stretch)
			->SetFlexGrow(1)
			->SetGap(8)
			->SetPadding(8);

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

		UIEndBox();
	}
}

void ConRegisterCommand(const char* name, int (*function)(int argc, const char** argv), const char* help)
{
	commands.push_back({ name, function, help });
}