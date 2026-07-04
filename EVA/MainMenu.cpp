#include <EVA/MainMenu.hpp>
#include <EVA/App.hpp>
#include <EVA/Platform.hpp>
#include <EVA/UI.hpp>

void MainMenuTick() {
	UIBeginBox()
		->SetPosition(0, 0)
		->SetSize(g_window_size.x, g_window_size.y)
		->SetFlex(UIAxis_Vertical, UIAlignment_Center, UIAlignment_Center)
		->SetGap(24);

	UILabel("gaming");


	UIBeginBox()
		->SetSize(200, 0)
		->SetFlex(UIAxis_Vertical, UIAlignment_Center, UIAlignment_Stretch)
		->SetGap(8);

	UIButton("New Game");
	UIButton("Find Servers");

	if (UIButton("Map Editor"))
	{
		AppSetMode(AppMode_Editor, nullptr);
	}

	UIButton("Settings");
	if (UIButton("Quit"))
	{
		exit(0);
	}

	UIEndBox();
	UIEndBox();
}