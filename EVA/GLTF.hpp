#pragma once

#include <EVA/GL.hpp>
#include <vector>

struct GLTF
{
	std::vector<Mesh*> meshes;
};

GLTF* GLTFLoad(const char* name);