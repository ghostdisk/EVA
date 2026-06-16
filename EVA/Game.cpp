#include <EVA/Game.hpp>

Game* ActiveGame = nullptr;

void GameInit(Game* game, const char* name)
{
	game->name = strdup(name);

	CameraInit(game->camera);
	game->camera.position.y = -10;
	game->camera.position.z = 3;

	EntityManagerInit(game->entity_manager);
}

void GameTick(Game* game, double dt)
{
	if (ActiveGame == game)
	{
		CameraFly(game->camera);
	}
	CameraUpdateMatrices(game->camera);
}