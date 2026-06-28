#include <EVA/Common.hpp>

struct Game;

enum AppMode
{
	AppMode_None,
	AppMode_Editor,
	AppMode_Game,
};

extern AppMode g_app_mode;
extern Game*   g_active_game;

void AppSetMode(AppMode mode, Game* game);