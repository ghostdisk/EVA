#pragma once
#include <EVA/Core/Common.hpp>

namespace EVA::GPU
{

enum class Backend
{
	NONE = 0,
	VULKAN,
	D3D12,
	METAL,
};

struct InitOptions
{
	Backend preferred_backend = Backend::NONE;
};

void Init(const InitOptions& init_options);
void Shutdown();

}