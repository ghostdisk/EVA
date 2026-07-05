#pragma once
#include <EVA/Net.hpp>
#include <EVA/Game.hpp>
#include <vector>

struct GameServerPlayer {
	ENetPeer* peer = nullptr;
};

struct GameServer {
	Game*                          game        = nullptr;
	ENetHost*                      host        = 0;
	std::vector<GameServerPlayer*> players     = {};
	EID                            next_eid    = 1;

	EID NewEID();
	void Init(Game* game, const char* ip, int port);
	void Tick(double dt);
	void HandlePlayerDisconnected(GameServerPlayer* player);
	bool Send(GameServerPlayer* player, const U8* message, size_t message_size);
	void Broadcast(const U8* message, size_t message_size);
};

void GameServerInitialize();
