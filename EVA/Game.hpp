#pragma once
#include <EVA/Entities/ECamera.hpp>
#include <EVA/Entities/EntityManager.hpp>
#include <box3d/box3d.h>

struct Mesh;
struct String;
struct Result;
struct GameServer;
struct GameClient;
struct EntityManager;

struct Game {
	int                id                     = 0;
	const char*        name                   = nullptr;
	EntityManager      entity_manager         = {};
	ECamera*           camera                 = {};
	Entity*            pawn                   = nullptr;
	Mesh*              level_mesh             = nullptr;
	GameServer*        server                 = nullptr;
	GameClient*        client                 = nullptr;
	b3WorldId          physics                = {};
	char               map_name[32]           = {};

	void Init();
	void Tick(double dt);
	void TickFixed(double dt);
	void Draw();

	Result LoadMap(String name);
	void UnloadMap();

	static void TickAll(double dt);
};

void GameInitialize();

extern Game*   g_active_game;