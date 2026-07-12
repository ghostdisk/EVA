#pragma once
#include <EVA/Entities/ECamera.hpp>
#include <EVA/Entities/EntityManager.hpp>
#include <box3d/box3d.h>

class Mesh;
class GameMode;

struct String;
struct Result;
struct GameServer;
struct GameClient;

class Game {
	GameMode*          m_gameMode             = nullptr;
	EntityManager      m_entityManager        = {};

	void EndGameMode();

public:
	int                id                     = 0;
	const char*        name                   = nullptr;
	ECamera*           m_activeCamera         = {};
	Mesh*              level_mesh             = nullptr;
	GameServer*        server                 = nullptr;
	GameClient*        client                 = nullptr;
	b3WorldId          physics                = {};

	void Init();
	void Tick(double dt);
	void Draw();
	void SetGameMode(Type* gm_class);

	static void TickAll(double dt);

	GameMode* GetGameMode() {
		return m_gameMode;
	}
};

void GameInitialize();
extern Game*   g_active_game;