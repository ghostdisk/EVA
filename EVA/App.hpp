#pragma once
#include <EVA/Core/Common.hpp>

struct Game;

enum AppMode {
	AppMode_None     = 0,
	AppMode_MainMenu = 1,
	AppMode_Editor   = 2,
	AppMode_Game     = 3,
};

extern AppMode g_app_mode;
extern Game*   g_active_game;

void AppSetMode(AppMode mode, Game* game);