#pragma once
#include <EVA/Net.hpp>

struct Game;

enum GameClientState
{
	GameClientState_Disconnected,
	GameClientState_Connecting,
	GameClientState_Connected,
};

struct GameClient
{
	Game*           game   = nullptr;
	GameClientState state  = GameClientState_Disconnected;
	ENetHost*       host   = 0;
	ENetPeer*       server = nullptr;
};

void GameClientInitialize();

void GameClientInit(GameClient* client, Game* game);
void GameClientConnect(GameClient* client, IPAddress ip, U16 port);
void GameClientDisconnect(GameClient* client, const char* reason);
void GameClientTick(GameClient* client, double dt);