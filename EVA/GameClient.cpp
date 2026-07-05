#include <EVA/Arena.hpp>
#include <EVA/GameClient.hpp>
#include <EVA/Result.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/Game.hpp>
#include <EVA/Console.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Binary.hpp>
#include <enet/enet.h>

Result Con_connect(ConParser& parser) {
	const char* address = parser.StringArg();
	if (!address || !address[0]) return Err("usage: connect ip:port");

	int a, b, c, d, port;
	if (sscanf(address, "%d.%d.%d.%d:%d", &a, &b, &c, &d, &port) != 5)
		return Err("usage: connect ip:port (e.g. connect 127.0.0.1:27015)");

	if (!g_active_game) return Err("no active game");
	if (g_active_game->server) return Err("[game %d] hosting a server, can't connect", g_active_game->id);
	if (g_active_game->client && g_active_game->client->state != GameClientState_Disconnected) return Err("[game %d] already connected, disconnect first", g_active_game->id);

	IPAddress ip;
	ip.octets[0] = a;
	ip.octets[1] = b;
	ip.octets[2] = c;
	ip.octets[3] = d;

	if (!g_active_game->client) {
		g_active_game->client = new GameClient();
		g_active_game->client->Init(g_active_game);
	}
	return g_active_game->client->Connect(ip, (U16)port);
}

Result Con_disconnect(ConParser& parser) {
	if (!g_active_game) return Err("no active game");
	if (!g_active_game->client) return Err("not connected");

	g_active_game->client->Disconnect(Err("Disconnected from console"));
	return Success();
}

void GameClientInitialize() {
	ConRegisterCommand("connect", Con_connect, "connect to a server (ip:port)");
	ConRegisterCommand("disconnect", Con_disconnect, "disconnect from the server");
}

void GameClient::Init(Game* game) {
	*this = {};
	this->game = game;
}

void GameClient::Disconnect(Result error) {
	assert(!error);
	Disconnect(*error.error, true);
}

void GameClient::Disconnect(String reason, bool error) {
	if (error) {
		ConError(Err("disconnected from server: %.*s", STRING_PRINTF_ARGS(reason)));
	} else {
		ConLog("disconnected from server: %.*s", STRING_PRINTF_ARGS(reason));
	}
	if (server) {
		enet_peer_disconnect_now(server, 0);
		server = nullptr;
	}
	if (host) {
		enet_host_destroy(host);
		host = nullptr;
	}
	state = GameClientState_Disconnected;
}

void GameClient::Tick(double dt) {
	if (state == GameClientState_Disconnected) return;

	if (host) {
		ENetEvent event;
		if (enet_host_service(host, &event, 0) > 0) {
			switch (event.type) {
				case ENET_EVENT_TYPE_CONNECT: {
					state = GameClientState_Connected;
					hello_received = false;
					ConLog("[game %d] Connected to server", game->id);
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT: {
					Disconnect("lost connection", true);
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE: {
					BinaryReader reader;
					BinaryReaderInit(reader, event.packet->data, event.packet->dataLength);

					Result res = HandlePacket(reader);
					if (!res) Disconnect(res);

					enet_packet_destroy(event.packet);
					break;
				}
				default: assert(0); break;
			}
		}
	}
}

Result GameClient::Connect(IPAddress ip, U16 port) {
	switch (state) {
		case GameClientState_Disconnected: break;
		case GameClientState_Connecting: return Err("already connecting to a server");
		case GameClientState_Connected: return Err("already connected to a server");
		default: assert(0); return Err("invalid state");
	}
	assert(!host);

	ENetAddress address = {};
	address.host = *(U32*)ip.octets;
	address.port = port;
	host = enet_host_create(nullptr, 1, NUM_CHANNELS, 0, 0);
	if (!host) return Err("enet_host_create failed"); 

	server = enet_host_connect(host, &address, NUM_CHANNELS, 0);
	if (!server) {
		enet_host_destroy(host);
		host = nullptr;
		return Err("[game %d] enet_host_connect failed", game->id);
	}

	state = GameClientState_Connecting;
	ConLog("[game %d] Connecting to %d.%d.%d.%d:%d", game->id, ip.octets[0], ip.octets[1], ip.octets[2], ip.octets[3], (int)port);
	return Success();
}

Result GameClient::HandleS2CHello(BinaryReader& reader) {
	if (hello_received) return Err("received hello multiple times");
	hello_received = true;

	String map_name = ReadBinString(reader, FrameArena, 32);
	TRY(game->LoadMap(map_name));

	return Success();
}

Result GameClient::HandleMessage(S2CMessageType mt, BinaryReader& reader) {
	switch (mt) {
		case S2CMessageType_Hello: return HandleS2CHello(reader);
		default: {
			return Err("received invalid message %d from server", mt);
		}
	}
}

Result GameClient::HandlePacket(BinaryReader& reader) {
	while (reader.ok) {
		S2CMessageType message = (S2CMessageType)ReadBinT<U8>(reader);

		if (message == 0) {
			if (reader.head == reader.end) {
				return Success();
			} else {
				return Err("received invalid message from server");
			}
		} else {
			TRY(HandleMessage(message, reader));
			if (!reader.ok) return Err("received invalid message from server");
		}

	}
	return Success();
}