#include <EVA/ServerGame.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Library.hpp>
#include <EVA/Library.hpp>
#include <enet/enet.h>
#include <stdio.h>

void ServerGameInit(ServerGame* game, const char* name)
{
	GameInit(game, name);

	for (int i = 0; i < 200; i++)
	{
		EStaticMesh* cube = game->entity_manager.StaticMesh.CreateEntity(i);
		cube->mesh = Library::mesh_cube;
		cube->position.z = 2 + i * 1.2;
		cube->position.x = 3 * (rand() % 100) / 100.0f;
		cube->position.y = 3 * (rand() % 100) / 100.0f;
		PhysicsAttachBodyToEntity(game->physics, cube, Library::shape_cube, PhysicsLayer_Moving);
	}

	EStaticMesh* ground = game->entity_manager.StaticMesh.CreateEntity(5000);
	ground->mesh = Library::mesh_cube;
	ground->scale.x = 20;
	ground->scale.y = 20;
	PhysicsAttachBodyToEntity(game->physics, ground, Library::shape_ground, PhysicsLayer_NonMoving);
}

void ServerGameTick(ServerGame* game, double dt)
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
					printf("[server] new connection!\n");
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
					printf("[server] new disconnection!\n");
				{
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE:
				{
					printf("[server] new messagenection!\n");
					enet_packet_destroy(event.packet);
					break;
				}
				default: {}
			}
		}
	}

	GameTick(game, dt);
}

void ServerListen(ServerGame* game, int port)
{
	ENetAddress address = {};
	address.host = ENET_HOST_ANY;
	address.port = port;
	game->host = enet_host_create(&address, MAX_CLIENTS, NUM_CHANNELS, 0, 0);
	if (!game->host)
	{
		Fatal("enet_host_create failed");
	}
}