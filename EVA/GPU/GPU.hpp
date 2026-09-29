#pragma once
#include <EVA/Core/Common.hpp>

namespace EVA::PAL
{
struct Window;
}

namespace EVA::GPU
{

struct Device
{
	void (*Shutdown)() = nullptr;
};

enum class Backend
{
	NONE = 0,
	D3D11,
};

struct InitOptions
{
	PAL::Window* window = nullptr;
	Backend preferred_backend = Backend::NONE;
};

struct BackendDesc
{
	Backend backend = Backend::NONE;
	bool (*Init)(Device& out_device, const InitOptions& options) = nullptr;
};

void Init(const InitOptions& init_options);
void Shutdown();

extern Device device;

inline void Shutdown()
{
	if (device.Shutdown)
	{
		device.Shutdown();
		device = {};
	}
}

}
