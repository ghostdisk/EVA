#pragma once
#include <EVA/Net.hpp>
#include <EVA/Game.hpp>
#include <vector>

struct ECharacter;

struct GameServerPlayer {
	ECharacter*      pawn       = nullptr;
	ENetPeer*        peer       = nullptr;
};

struct GameServer {
	Game*                          game        = nullptr;
	ENetHost*                      host        = 0;
	std::vector<GameServerPlayer*> players     = {};
	EID                            next_eid    = EID_SyncedStart;

	EID NewEID();
	void Init(Game* game, const char* ip, int port);
	void Tick(double dt);
	void HandlePlayerDisconnected(GameServerPlayer* player);
	bool Send(GameServerPlayer* player, const U8* message, size_t message_size);
	void Broadcast(const U8* message, size_t message_size);

	void SendHello(GameServerPlayer* player);
};

void GameServerInitialize();
