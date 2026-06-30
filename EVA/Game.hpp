#pragma once
#include <EVA/Entities.hpp>
#include <EVA/Camera.hpp>
#include <vector>

struct GLTFScene;
struct CSGBrush;
struct PhysicsWorld;

struct Game
{
	int           id              = 0;
	const char*   name            = nullptr;
	EntityManager entity_manager  = {};
	Camera        camera          = {};
	Entity*       pawn            = nullptr;
	PhysicsWorld* physics         = nullptr;
};

void GameInitialize();

void GameInit(Game* game);
void GameTick(Game* game, double dt);
void GameDraw(Game* game);
void GameTickAll(double dt);
void GameLoadMap(Game* game, const char* map);

EID InstantiateScene(Game* game, GLTFScene* scene, EID start_eid);