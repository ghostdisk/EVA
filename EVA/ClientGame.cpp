#include <EVA/ClientGame.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Wire.hpp>
#include <EVA/Binary.hpp>
#include <enet/enet.h>

/*

void ClientGameInit(ClientGame* game, const char* name)
{
	GameInit(game, name);
}

void Disconnect(ClientGame* game, const char* reason)
{
	Fatal("Disconnect: %s", reason); // TODO
}

void ClientGameTick(ClientGame* game, double dt)
{

	if (game->host)
	{
		ENetEvent event;
		if (enet_host_service(game->host, &event, 0) > 0)
		{
			switch (event.type)
			{
				case ENET_EVENT_TYPE_CONNECT:
				{
					printf("[client] new connection!\n");
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
					printf("[client] new disconnection!\n");
				{
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
									return Disconnect(game, "Received invalid message from server");
								}
								U32 eid = ReadBinT<U32>(reader);
								Entity* entity = game->entity_manager.CreateEntity(type, eid);
								if (!entity)
								{
									return Disconnect(game, "Failed to create an enitty");
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
										return Disconnect(game, "Server requested invalid asset");
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
				default: {}
			}
		}
	}

	GameTick(game, dt);
}

void ClientConnect(ClientGame* game, IPAddress ip, U16 port)
{
	ENetAddress address = {};
	address.host = *(U32*)ip.octets;
	address.port = port;
	game->host = enet_host_create(nullptr, 1, NUM_CHANNELS, 0, 0);
	if (!game->host)
	{
		Fatal("enet_host_create failed");
	}

	game->server = enet_host_connect(game->host, &address, NUM_CHANNELS, 0);
}
*/