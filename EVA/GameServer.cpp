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

void Con_host(ConParser& parser) {
	if (!g_active_game)        return ConError("no active game");
	if (g_active_game->server) return ConError("[game %d] already hosting", g_active_game->id);
	if (g_active_game->client) return ConError("[game %d] connected as a client, disconnect first", g_active_game->id);

	int port = parser.IntArg(27015);

	g_active_game->server = new GameServer();
	GameServerInit(g_active_game->server, g_active_game, nullptr, port);
	ConLog("[game %d] Hosting at %d", g_active_game->id, port);
}

void GameServerInitialize() {
	ConRegisterCommand("host", Con_host, "host a server on a port");
}

static EID NewEID(GameServer* server) {
	return server->next_eid++;
}

void GameServerInit(GameServer* server, Game* game, const char* ip, int port) {
	server->game = game;

	ENetAddress address = {};
	address.host = ENET_HOST_ANY;
	if (ip) enet_address_set_host_ip(&address, ip);
	address.port = port;
	server->host = enet_host_create(&address, MAX_CLIENTS, NUM_CHANNELS, 0, 0);

	if (!server->host) Fatal("enet_host_create failed");
}

static void OnPlayerDisconnected(GameServer* server, GameServerPlayer* player) {
}

void GameServerTick(GameServer* server, double dt) {
	if (!server->host) return;

	ENetEvent event;
	while (enet_host_service(server->host, &event, 0) > 0) {
		switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT: {
				GameServerPlayer* player = new GameServerPlayer();
				player->peer = event.peer;
				event.peer->userdata = player;
				server->players.push_back(player);
				ConLog("[game %d] player connected", server->game->id);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT: {
				GameServerPlayer* player = (GameServerPlayer*)event.peer->userdata;
				OnPlayerDisconnected(server, player);

				for (int i = 0; i < server->players.size(); i++) {
					if (server->players[i] == player) {
						server->players[i] = server->players.back();
						server->players.pop_back();
						break;
					}
				}
				event.peer->userdata = nullptr;
				delete player;
				ConLog("[game %d] player disconnected", server->game->id);
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

static bool Send(GameServerPlayer* player, const U8* message, size_t message_size) {
	ENetPacket* packet = enet_packet_create(message, message_size, ENET_PACKET_FLAG_RELIABLE);
	if (enet_peer_send(player->peer, 0, packet) == 0) {
		return true;
	} else {
		enet_packet_destroy(packet);
		return false;
	}
}

static void Broadcast(GameServer* server, const U8* message, size_t message_size) {
	for (GameServerPlayer* player : server->players) {
		Send(player, message, message_size);
	}
}

static void FillOutEntityCreateMessage(BinaryWriter& writer, Entity* entity) {
	WriteBinT<U8>(writer, S2CMessageType_EntityCreate);
	WriteBinT<U8>(writer, entity->type);
	WriteBinT<U32>(writer, entity->eid);
	WriteBinT<float3>(writer, entity->position);
	WriteBinT<float4>(writer, entity->rotation);
	WriteBinT<float3>(writer, entity->scale);
	WriteBinT<U32>(writer, entity->mesh ? entity->mesh->id : 0);
}

static void SendHello(GameServer* server, GameServerPlayer* player) {
	BinaryWriter writer;
	BinaryWriterInit(writer);

	server->game->entity_manager.Iterate([&](Entity* entity) {
		FillOutEntityCreateMessage(writer, entity);
	});

	printf("[server] Sending %d bytes hello\n", (int)writer.data.size());
	Send(player, writer.data.data(), writer.data.size());
}