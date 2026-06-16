#pragma once
#include <EVA/Entities.hpp>
#include <EVA/Camera.hpp>

struct Physics;


struct Game
{
	const char*   name            = nullptr;
	EntityManager entity_manager  = {};
	Camera        camera          = {};
	Physics*      physics         = nullptr;
};

extern Game* ActiveGame;

void GameInit(Game* game, const char* name);
void GameTick(Game* game, double dt);