#include <EVA/ClientGame.hpp>
#include <EVA/GLTF.hpp>
#include <enet/enet.h>

extern GLTF* gltf_monke;

void ClientGameInit(ClientGame* game, const char* name)
{
	GameInit(game, name);

	EStaticMesh* monkey1 = game->entity_manager.StaticMesh.CreateEntity(1);
	monkey1->mesh = gltf_monke->meshes[0];
	monkey1->position.x = 4;

	EStaticMesh* monkey2 = game->entity_manager.StaticMesh.CreateEntity(1);
	monkey2->mesh = gltf_monke->meshes[0];
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