/*
#include <EVA/ServerGame.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Library.hpp>
#include <EVA/Binary.hpp>
#include <EVA/Wire.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Character.hpp>
#include <EVA/GL.hpp> // Mesh
#include <EVA/CSG.hpp> // Mesh
#include <enet/enet.h>
#include <stdio.h>

static EID NewEID(ServerGame* server)
{
	return server->next_eid++;
}

void ServerGameInit(ServerGame* server, const char* name)
{
	GameInit(server, name);
}

static void OnPlayerDisconnected(ServerGame* server, ServerPlayer* player)
{
}

static bool Send(ServerPlayer* player, const U8* message, size_t message_size)
{
	ENetPacket* packet = enet_packet_create(message, message_size, ENET_PACKET_FLAG_RELIABLE);
	if (enet_peer_send(player->peer, 0, packet) == 0)
	{
		return true;
	}
	else
	{
		enet_packet_destroy(packet);
		return false;
	}
}

static void Broadcast(ServerGame* server, const U8* message, size_t message_size)
{
	for (ServerPlayer* player : server->players)
	{
		Send(player, message, message_size);
	}
}

static void FillOutEntityCreateMessage(BinaryWriter& writer, Entity* entity)
{
	WriteBinT<U8>(writer, S2CMessageType_EntityCreate);
	WriteBinT<U8>(writer, entity->type);
	WriteBinT<U32>(writer, entity->eid);
	WriteBinT<float3>(writer, entity->position);
	WriteBinT<float4>(writer, entity->rotation);
	WriteBinT<float3>(writer, entity->scale);
	WriteBinT<U32>(writer, entity->mesh ? entity->mesh->id : 0);
}

static void SendHello(ServerGame* server, ServerPlayer* player)
{
	BinaryWriter writer;
	BinaryWriterInit(writer);

	server->entity_manager.Iterate(
		[&](Entity* entity)
		{
			FillOutEntityCreateMessage(writer, entity);
		});

	printf("[server] Sending %d bytes hello\n", (int)writer.data.size());
	Send(player, writer.data.data(), writer.data.size());
}


void ServerGameTick(ServerGame* server, double dt)
{
	if (server->host)
	{
		ENetEvent event;
		if (enet_host_service(server->host, &event, 0) > 0)
		{
			switch (event.type)
			{
				case ENET_EVENT_TYPE_CONNECT:
				{
					ServerPlayer* player = new ServerPlayer();
					player->peer = event.peer;
					event.peer->userdata = player;
					SendHello(server, player);
					server->players.push_back(player);
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
				{
					ServerPlayer* player = (ServerPlayer*)event.peer->userdata;
					OnPlayerDisconnected(server, player);

					for (int i = 0; i < server->players.size(); i++)
					{
						if (server->players[i] == player)
						{
							server->players[i] = server->players.back();
							server->players.pop_back();
							break;
						}
					}
					delete player;
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE:
				{
					printf("[server] new message!\n");
					enet_packet_destroy(event.packet);
					break;
				}
				default: {}
			}
		}
	}

	GameTick(server, dt);
	if (server->pawn)
	{
		CharacterDoMovement(server, (ECharacter*)server->pawn, dt);
		CameraOrbit(server->camera, server->pawn);
		CameraUpdateMatrices(server->camera);
	}
}

void ServerListen(ServerGame* server, int port)
{
	ENetAddress address = {};
	address.host = ENET_HOST_ANY;
	address.port = port;
	server->host = enet_host_create(&address, MAX_CLIENTS, NUM_CHANNELS, 0, 0);
	if (!server->host)
	{
		Fatal("enet_host_create failed");
	}
}
*/