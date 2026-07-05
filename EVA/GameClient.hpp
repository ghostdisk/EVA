#pragma once
#include <EVA/Net.hpp>

struct Game;
struct Result;

enum GameClientState {
	GameClientState_Disconnected,
	GameClientState_Connecting,
	GameClientState_Connected,
};

struct GameClient {
	Game*           game   = nullptr;
	GameClientState state  = GameClientState_Disconnected;
	ENetHost*       host   = 0;
	ENetPeer*       server = nullptr;

	void Init(Game* game);
	Result Connect(IPAddress ip, U16 port);
	Result Disconnect(const char* reason);
	void Tick(double dt);
};

void GameClientInitialize();
