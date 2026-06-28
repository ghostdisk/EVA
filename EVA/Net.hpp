#pragma once
#include <EVA/Common.hpp>

#define MAX_CLIENTS 32
#define NUM_CHANNELS 2

typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

struct IPAddress
{
	U8 octets[4] = {0,0,0,0};
};

void NetInitialize();