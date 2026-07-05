#pragma once
#include <EVA/Entities.hpp>
#include <EVA/Camera.hpp>
#include <box3d/box3d.h>

struct Result;
struct GameServer;
struct GameClient;

struct Game {
	int                id                     = 0;
	const char*        name                   = nullptr;
	EntityManager      entity_manager         = {};
	Camera             camera                 = {};
	Entity*            pawn                   = nullptr;
	Mesh*              level_mesh             = nullptr;
	GameServer*        server                 = nullptr;
	GameClient*        client                 = nullptr;
	b3WorldId          physics                = {};

	void Init();
	void Tick(double dt);
	void Draw();

	Result LoadMap(const char* map);
	void UnloadMap();

	static void TickAll(double dt);
};

void GameInitialize();

extern Game*   g_active_game;