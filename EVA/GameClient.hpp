#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Net.hpp>

class  Game;
struct BinaryReader;

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
	bool            hello_received = false;

	void Init(Game* game);
	Result Connect(IPAddress ip, U16 port);
	void Disconnect(String reason, bool error);
	void Disconnect(Result error);
	void Tick(double dt);

	Result HandlePacket(BinaryReader& reader);
	Result HandleMessage(S2CMessageType mt, BinaryReader& reader);
	Result HandleS2CHello(BinaryReader& reader);
};

void GameClientInitialize();
