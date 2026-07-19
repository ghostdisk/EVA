#include <EVA/Assets/Model.hpp>
#include <EVA/GFX/Mesh.hpp>
#include <EVA/Core/Serialization.hpp>
#include <EVA/GFX/Mesh.hpp>

Result Model::LoadImpl(Deserializer& d) {
	ModelData data;
	Deserialize(d, data);
	TRY(d.res);

	for (const ModelMeshData& serializedMesh : data.meshes) {
		MeshAsset* mesh = (MeshAsset*)GetOrCreateChild(serializedMesh.name, MeshAsset::StaticClass());

		if (!mesh->m_mesh) mesh->m_mesh = new Mesh();
		mesh->m_mesh->Unload();
		mesh->m_mesh->Upload(serializedMesh.meshData);
	}

	return Success();
}

