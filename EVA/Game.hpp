#pragma once
#include <EVA/Entities.hpp>
#include <EVA/Camera.hpp>
#include <EVA/Physics.hpp>
#include <vector>

struct GLTFScene;
struct CSGBrush;
struct PhysicsWorld;
struct GameServer;
struct GameClient;

struct Game {
	int                id                     = 0;
	const char*        name                   = nullptr;
	EntityManager      entity_manager         = {};
	Camera             camera                 = {};
	Entity*            pawn                   = nullptr;
	PhysicsWorld*      physics                = nullptr;
	Mesh*              level_mesh             = nullptr;
	PhysicsCollider    level_mesh_collider    = {};
	GameServer*        server                 = nullptr;
	GameClient*        client                 = nullptr;
};

void GameInitialize();

void GameInit(Game* game);
void GameTick(Game* game, double dt);
void GameDraw(Game* game);
void GameTickAll(double dt);
void GameLoadMap(Game* game, const char* map);

EID InstantiateScene(Game* game, GLTFScene* scene, EID start_eid);

extern Game*   g_active_game;