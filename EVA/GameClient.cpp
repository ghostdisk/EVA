#include <EVA/GameClient.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/Game.hpp>
#include <EVA/Console.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Wire.hpp>
#include <EVA/Binary.hpp>
#include <enet/enet.h>

void Con_connect(ConParser& parser) {
	const char* address = parser.StringArg();
	if (!address || !address[0]) {
		ConError("usage: connect ip:port");
		return;
	}

	int a, b, c, d, port;
	if (sscanf(address, "%d.%d.%d.%d:%d", &a, &b, &c, &d, &port) != 5) {
		ConError("usage: connect ip:port (e.g. connect 127.0.0.1:27015)");
		return;
	}

	if (!g_active_game) return ConError("no active game");
	if (g_active_game->server) return ConError("[game %d] hosting a server, can't connect", g_active_game->id);
	if (g_active_game->client && g_active_game->client->state != GameClientState_Disconnected) return ConError("[game %d] already connected, disconnect first", g_active_game->id);

	IPAddress ip;
	ip.octets[0] = a;
	ip.octets[1] = b;
	ip.octets[2] = c;
	ip.octets[3] = d;

	if (!g_active_game->client) {
		g_active_game->client = new GameClient();
		GameClientInit(g_active_game->client, g_active_game);
	}
	GameClientConnect(g_active_game->client, ip, (U16)port);
}

void Con_disconnect(ConParser& parser) {
	if (g_active_game && g_active_game->client) {
		GameClientDisconnect(g_active_game->client, "Disconnected from console");
	}
}

void GameClientInitialize() {
	ConRegisterCommand("connect", Con_connect, "connect to a server (ip:port)");
	ConRegisterCommand("disconnect", Con_disconnect, "disconnect from the server");
}

void GameClientInit(GameClient* client, Game* game) {
	*client = {};
	client->game = game;
}

void GameClientDisconnect(GameClient* client, const char* reason) {
	if (client->state == GameClientState_Disconnected) return;

	ConLog("[game %d] Disconnected: %s", client->game->id, reason);

	if (client->server) {
		enet_peer_disconnect_now(client->server, 0);
		client->server = nullptr;
	}
	if (client->host) {
		enet_host_destroy(client->host);
		client->host = nullptr;
	}
	client->state = GameClientState_Disconnected;
}

void GameClientTick(GameClient* client, double dt) {
	if (client->state == GameClientState_Disconnected) return;

	if (client->host) {
		ENetEvent event;
		if (enet_host_service(client->host, &event, 0) > 0) {
			switch (event.type) {
				case ENET_EVENT_TYPE_CONNECT: {
					client->state = GameClientState_Connected;
					ConLog("[game %d] Connected to server", client->game->id);
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT: {
					GameClientDisconnect(client, "enet dc");
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE: {
					printf("[client] received %d bytes!\n", (int)event.packet->dataLength);

					BinaryReader reader;
					BinaryReaderInit(reader, event.packet->data, event.packet->dataLength);

					while (reader.ok) {
						S2CMessageType message = (S2CMessageType)ReadBinT<U8>(reader);
						switch (message) {
							case S2CMessageType_EntityCreate: {
								EntityType type = (EntityType)ReadBinT<U8>(reader);
								if (type >= EntityType_ENUM_SIZE || type == 0) {
									return GameClientDisconnect(client, "Received invalid message from server");
								}
								U32 eid = ReadBinT<U32>(reader);
								Entity* entity = client->game->entity_manager.CreateEntity(type, eid);
								if (!entity) {
									return GameClientDisconnect(client, "Failed to create an enitty");
								}
								entity->position = ReadBinT<float3>(reader);
								entity->rotation = ReadBinT<float4>(reader);
								entity->scale = ReadBinT<float3>(reader);

								EStaticMesh* static_mesh = (EStaticMesh*)entity;
								U32 mesh_id = ReadBinT<U32>(reader);
								if (mesh_id > 0) {
									Mesh* mesh = (Mesh*)AssetGet(mesh_id, AssetType_Mesh);
									if (!mesh) {
										return GameClientDisconnect(client, "Server requested invalid asset");
									}
									static_mesh->mesh = mesh;
								}
								break;
							}
							default: {
								assert(message == 0); // we get 0 if the message is over. if this blows, we have a serialization issue.
								reader.ok = false;
								break;
							}
						}
					}

					enet_packet_destroy(event.packet);
					break;
				}
				default: assert(0); break;
			}
		}
	}
}

void GameClientConnect(GameClient* client, IPAddress ip, U16 port) {
	assert(client->state == GameClientState_Disconnected);
	assert(!client->host);

	ENetAddress address = {};
	address.host = *(U32*)ip.octets;
	address.port = port;
	client->host = enet_host_create(nullptr, 1, NUM_CHANNELS, 0, 0);
	if (!client->host) {
		Fatal("enet_host_create failed");
	}

	client->server = enet_host_connect(client->host, &address, NUM_CHANNELS, 0);
	if (!client->server) {
		enet_host_destroy(client->host);
		client->host = nullptr;
		ConError("[game %d] enet_host_connect failed", client->game->id);
		return;
	}

	client->state = GameClientState_Connecting;
	ConLog("[game %d] Connecting to %d.%d.%d.%d:%d", client->game->id, ip.octets[0], ip.octets[1], ip.octets[2], ip.octets[3], (int)port);
}