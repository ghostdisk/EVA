#include <EVA/ClientGame.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Physics.hpp>
#include <enet/enet.h>

extern GLTF* gltf_monke;
extern GLTF* gltf_cube;

void ClientGameInit(ClientGame* game, const char* name)
{
	GameInit(game, name);

	PhysicsShape* shape_cube1x1 = PhysicsCreateBoxShape(float3(1,1,1));
	PhysicsShape* shape_floor = PhysicsCreateBoxShape(float3(20,1,20));

	for (int i = 0; i < 30; i++)
	{
		EStaticMesh* cube = game->entity_manager.StaticMesh.CreateEntity(i);
		printf("%p\n", cube);
		cube->mesh = gltf_cube->meshes[0];
		cube->position.z = 2 + i * 2.2;
		cube->position.x = (rand() % 100) / 100.0f;
		cube->position.y = (rand() % 100) / 100.0f;
		PhysicsAttachBodyToEntity(game->physics, cube, shape_cube1x1, PhysicsLayer_Moving);
	}

	EStaticMesh* floor = game->entity_manager.StaticMesh.CreateEntity(100);
	floor->mesh = gltf_cube->meshes[0];
	floor->scale.x = 20;
	floor->scale.y = 20;
	PhysicsAttachBodyToEntity(game->physics, floor, shape_floor, PhysicsLayer_NonMoving);
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
					printf("[client] new messagenection!\n");
					break;
				}
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