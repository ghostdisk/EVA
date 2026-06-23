#include <EVA/Arena.hpp>
#include <EVA/Console.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <EVA/Platform.hpp>
#include <string>
#include <vector>
#include <stdarg.h>

std::vector<char> console_input;
std::vector<std::string> console_log;
static bool console_open = false;


void ConLog(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const char* str = ArenaVprintf(FrameArena, fmt, args);
	va_end(args);
	console_log.push_back(str);
}

void ConExec(const char* cmd)
{
	ConLog(cmd);
}

void ConsoleDraw()
{
	bool just_opened = false;
	if (InputGetButtonDown(SDL_SCANCODE_GRAVE))
	{
		console_open = !console_open;
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

		{ // input bar
			UIBeginBox()
				->SetFlex(UIAxis_Horizontal, UIAlignment_Stretch, UIAlignment_Stretch)
				->SetGap(8);

			UIPushId("input");
			UIBox* text_input = UITextInput(console_input)->SetFlexGrow(1);
			if (just_opened) UIFocus(text_input);
			UIPopId();

			if (UIButton("Submit") || ((text_input->flags & UIBoxFlags_Focus) && InputGetButtonDown(SDL_SCANCODE_RETURN)))
			{
				ConExec(ArenaInternCString(FrameArena, console_input.data(), console_input.size()));
				console_input.clear();
			}
			UIEndBox();
		}
		UIEndBox();
	}

}