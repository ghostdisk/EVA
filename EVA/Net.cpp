
#include <EVA/Net.hpp>
#include <enet/enet.h>

void NetInitialize() {
	if (enet_initialize() != 0) {
		Fatal("enet_initialize failed");
	}
}
