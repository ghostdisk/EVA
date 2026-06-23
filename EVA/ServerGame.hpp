#pragma once
/*
#include <EVA/Game.hpp>
#include <EVA/Net.hpp>
#include <vector>


struct ServerPlayer
{
	ENetPeer* peer = nullptr;
};

struct ServerGame : Game
{
	ENetHost*                  host     = 0;
	std::vector<ServerPlayer*> players  = {};
	EID                        next_eid = 1;
};

void ServerGameInit(ServerGame* game, const char* name);
void ServerGameTick(ServerGame* game, double dt);
void ServerListen(ServerGame* game, int port);
*/