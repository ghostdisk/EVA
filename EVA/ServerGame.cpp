#include <EVA/ServerGame.hpp>
#include <enet/enet.h>
#include <stdio.h>


void ServerGameInit(ServerGame* game, const char* name)
{
	GameInit(game, name);

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