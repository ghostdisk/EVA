#pragma once
#include <EVA/Entities.hpp>
#include <EVA/Camera.hpp>

struct Game
{
	const char*   name            = nullptr;
	EntityManager entity_manager  = {};
	Camera        camera          = {};
};

extern Game* ActiveGame;

void GameInit(Game* game, const char* name);
void GameTick(Game* game, double dt);