#include <EVA/ClientGame.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Physics.hpp>
#include <enet/enet.h>


void ClientGameInit(ClientGame* game, const char* name)
{
	GameInit(game, name);
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
					enet_packet_destroy(event.packet);
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