#pragma once
#include <EVA/Math.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/Renderer/GraphicsDevice.hpp>
#include <vector>

class Material;

// TODO: FIgure out what the fuck is wrong with the autoserializer
struct MeshVertex {
	float3 position;
	float3 normal;
	float2 texcoord;
};

void Serialize(Serializer& s, const MeshVertex& f);
void Deserialize(Deserializer& d, MeshVertex& f);

class Mesh : public Asset {
public:
	ECLASS_COMMON();

	Material*     default_material = nullptr;

	// cpu data:
	std::vector<MeshVertex> vertices;
	std::vector<U32>        indices;

	// gpu data, initialized by Upload()
	GFX::GPUBuffer* vertex_buffer = nullptr;
	GFX::GPUBuffer* index_buffer  = nullptr;
	U32             index_count   = 0;
	U32             vertex_count  = 0;

	void InitCPUData(size_t num_vertices, const MeshVertex* vertices, size_t num_indices, const U32* indices);
	void Upload(bool keep_cpu_data);
	void Deinit(); // destroys gpu data
};

void Serialize(Serializer& s, Mesh* value);
void Deserialize(Deserializer& d, Mesh* value);
