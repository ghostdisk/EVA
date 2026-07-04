#pragma once
#include <EVA/Net.hpp>
#include <EVA/Game.hpp>
#include <vector>

struct GameServerPlayer
{
	ENetPeer* peer = nullptr;
};

struct GameServer
{
	Game*                          game        = nullptr;
	ENetHost*                      host        = 0;
	std::vector<GameServerPlayer*> players     = {};
	EID                            next_eid    = 1;
};

void GameServerInitialize();

void GameServerInit(GameServer* server, Game* game, const char* ip, int port);
void GameServerTick(GameServer* server, double dt);