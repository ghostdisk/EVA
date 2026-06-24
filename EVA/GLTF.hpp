#pragma once

#include <EVA/Renderer/GL.hpp>
#include <vector>

struct EntityManager;

struct GLTFSceneNode
{
	char      name[16]  = {};
	float3    position  = {};
	float4    rotation  = {0,0,0,1};
	float3    scale     = {1,1,1};
	Mesh*     mesh      = nullptr;
	Material* material  = nullptr;
};

struct GLTFScene
{
	std::vector<GLTFSceneNode> nodes;
};

struct GLTF
{
	std::vector<Mesh*>      meshes     = {};
	std::vector<Material*>  materials  = {};
	std::vector<GLTFScene*> scenes     = {};
};

GLTF* GLTFLoad(const char* name, bool generate_colliders);