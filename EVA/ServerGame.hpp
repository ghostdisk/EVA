#pragma once
#include <EVA/Game.hpp>
#include <EVA/Net.hpp>

struct ServerGame : Game
{
	ENetHost* host = 0;
};

void ServerGameInit(ServerGame* game, const char* name);
void ServerGameTick(ServerGame* game, double dt);
void ServerListen(ServerGame* game, int port);