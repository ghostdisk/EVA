#pragma once
#include <EVA/Assets/Asset.hpp>
#include <EVA/Assets/MeshAsset.hpp>
#include <EVA/GFX/Mesh.hpp>

struct EVERSION(1) ModelMeshData {
	String name;
	MeshData meshData;
};
EAUTO_SERIALIZE(ModelMeshData);

struct EVERSION(1) ModelData {
	Vector<ModelMeshData> meshes;
};
EAUTO_SERIALIZE(ModelData);

class Model : public Asset {
public:
	ECLASS_COMMON(Model)

	Vector<MeshAsset*> m_meshes;

	virtual Vector<String> GetFileExtensions() override {
		return { ".mdl" };
	}
	virtual Result LoadImpl(Deserializer& d) override;
};
