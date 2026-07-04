#include <EVA/GameClient.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/Game.hpp>
#include <EVA/Console.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Wire.hpp>
#include <EVA/Binary.hpp>
#include <enet/enet.h>

void Con_connect(ConParser& parser)
{
	if (g_active_game)
	{
		assert(!g_active_game->client);
		assert(!g_active_game->server);
		g_active_game->client = new GameClient();
		GameClientInit(g_active_game->client, g_active_game);
	}
}

void Con_disconnect(ConParser& parser)
{
	if (g_active_game && g_active_game->client)
	{
		GameClientDisconnect(g_active_game->client, "Disconnected from console");
	}
}

void GameClientInitialize()
{
	ConRegisterCommand("connect", Con_connect);
	ConRegisterCommand("GameClientDisconnect", Con_disconnect);
}

void GameClientInit(GameClient* client, Game* game)
{
	*client = {};
}

void GameClientDisconnect(GameClient* client, const char* reason)
{
	client->state = GameClientState_Disconnected;
	Fatal("TODO: GameClientDisconnect (reason: %s)", reason); 
}

void GameClientTick(GameClient* client, double dt)
{
	if (client->state == GameClientState_Disconnected) return;

	if (client->host)
	{
		ENetEvent event;
		if (enet_host_service(client->host, &event, 0) > 0)
		{
			switch (event.type)
			{
				case ENET_EVENT_TYPE_CONNECT:
				{
					client->state = GameClientState_Connected;
					ConLog("[game %d] Connected to server", client->game->id);
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
				{
					GameClientDisconnect(client, "enet dc");
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE:
				{
					printf("[client] received %d bytes!\n", (int)event.packet->dataLength);

					BinaryReader reader;
					BinaryReaderInit(reader, event.packet->data, event.packet->dataLength);

					while (reader.ok)
					{
						S2CMessageType message = (S2CMessageType)ReadBinT<U8>(reader);
						switch (message)
						{
							case S2CMessageType_EntityCreate:
							{
								EntityType type = (EntityType)ReadBinT<U8>(reader);
								if (type >= EntityType_ENUM_SIZE || type == 0)
								{
									return GameClientDisconnect(client, "Received invalid message from server");
								}
								U32 eid = ReadBinT<U32>(reader);
								Entity* entity = client->game->entity_manager.CreateEntity(type, eid);
								if (!entity)
								{
									return GameClientDisconnect(client, "Failed to create an enitty");
								}
								entity->position = ReadBinT<float3>(reader);
								entity->rotation = ReadBinT<float4>(reader);
								entity->scale = ReadBinT<float3>(reader);

								EStaticMesh* static_mesh = (EStaticMesh*)entity;
								U32 mesh_id = ReadBinT<U32>(reader);
								if (mesh_id > 0)
								{
									Mesh* mesh = (Mesh*)AssetGet(mesh_id, AssetType_Mesh);
									if (!mesh)
									{
										return GameClientDisconnect(client, "Server requested invalid asset");
									}
									static_mesh->mesh = mesh;
								}
								break;
							}
							default:
							{
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

void GameClientConnect(GameClient* client, IPAddress ip, U16 port)
{
	ENetAddress address = {};
	address.host = *(U32*)ip.octets;
	address.port = port;
	client->host = enet_host_create(nullptr, 1, NUM_CHANNELS, 0, 0);
	if (!client->host)
	{
		Fatal("enet_host_create failed");
	}

	client->server = enet_host_connect(client->host, &address, NUM_CHANNELS, 0);
	client->state = GameClientState_Connecting;
}