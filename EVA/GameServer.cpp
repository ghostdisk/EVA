#include <EVA/GameServer.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/Console.hpp>
#include <EVA/Library.hpp>
#include <EVA/Binary.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Renderer/GL.hpp> // Mesh
#include <EVA/CSG.hpp> // Mesh
#include <enet/enet.h>
#include <stdio.h>

Result Con_host(ConParser& parser) {
	if (!g_active_game)        return Err("no active game");
	if (g_active_game->server) return Err("[game %d] already hosting", g_active_game->id);
	if (g_active_game->client) return Err("[game %d] connected as a client, disconnect first", g_active_game->id);

	int port = parser.IntArg(27015);

	g_active_game->server = new GameServer();
	g_active_game->server->Init(g_active_game, nullptr, port);
	ConLog("[game %d] Hosting at %d", g_active_game->id, port);
	return Success();
}

void GameServerInitialize() {
	ConRegisterCommand("host", Con_host, "host a server on a port");
}

EID GameServer::NewEID() {
	return next_eid++;
}

void GameServer::Init(Game* game, const char* ip, int port) {
	this->game = game;

	ENetAddress address = {};
	address.host = ENET_HOST_ANY;
	if (ip) enet_address_set_host_ip(&address, ip);
	address.port = port;
	host = enet_host_create(&address, MAX_CLIENTS, NUM_CHANNELS, 0, 0);

	if (!host) Fatal("enet_host_create failed");
}

void GameServer::HandlePlayerDisconnected(GameServerPlayer* player) {
}

bool GameServer::Send(GameServerPlayer* player, const U8* message, size_t message_size) {
	ENetPacket* packet = enet_packet_create(message, message_size, ENET_PACKET_FLAG_RELIABLE);
	if (enet_peer_send(player->peer, 0, packet) == 0) {
		return true;
	} else {
		enet_packet_destroy(packet);
		return false;
	}
}

void GameServer::Broadcast(const U8* message, size_t message_size) {
	for (GameServerPlayer* player : players) {
		Send(player, message, message_size);
	}
}

void GameServer::SendHello(GameServerPlayer* player) {
	BinaryWriter writer;
	BinaryWriterInit(writer);

	WriteBinT<U8>(writer, S2CMessageType_Hello);
	WriteBinString(writer, game->map_name);

	Send(player, writer.data.data(), writer.data.size());
}

void GameServer::Tick(double dt) {
	if (!host) return;

	ENetEvent event;
	while (enet_host_service(host, &event, 0) > 0) {
		switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT: {
				GameServerPlayer* player = new GameServerPlayer();
				player->peer = event.peer;
				event.peer->userdata = player;
				players.push_back(player);
				ConLog("[game %d] player connected", game->id);
				SendHello(player);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT: {
				GameServerPlayer* player = (GameServerPlayer*)event.peer->userdata;
				this->HandlePlayerDisconnected(player);

				for (int i = 0; i < players.size(); i++) {
					if (players[i] == player) {
						players[i] = players.back();
						players.pop_back();
						break;
					}
				}
				event.peer->userdata = nullptr;
				delete player;
				ConLog("[game %d] player disconnected", game->id);
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE: {
				printf("[server] new message!\n");
				enet_packet_destroy(event.packet);
				break;
			}
			default: break;
		}
	}
}