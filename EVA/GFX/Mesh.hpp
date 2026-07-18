#pragma once
#include <EVA/Math.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/GFX/GPUDevice.hpp>

class Material;

/**
 ** MeshVertex
 **/
struct MeshVertex {
	float3 position;
	float3 normal;
	float2 texcoord;
};
void Serialize(Serializer& s, const MeshVertex& f);
void Deserialize(Deserializer& d, MeshVertex& f);


/**
 ** MeshData represents the mesh data in regular RAM.
 ** This can be serialized/deserialized or passed to a Mesh to be uploaded.
 **/
class MeshData {
public:
	Vector<MeshVertex> vertices;
	Vector<U32>        indices;
};
void Serialize(Serializer& s, const MeshData& value);
void Deserialize(Deserializer& d, MeshData& value);


/**
 ** Mesh represents a mesh, loaded in the GPU
 **/
class Mesh : Object {
public:
	ECLASS_COMMON(Mesh);

	Material* m_defaultMaterial = nullptr;

	// gpu data, initialized by Upload()
	GPUBuffer*      m_vertexBuffer = nullptr;
	GPUBuffer*      m_indexBuffer  = nullptr;
	U32             m_indexCount   = 0;
	U32             m_vertexCount  = 0;

	void Upload(const MeshData& data);
	void Unload();
};

