#pragma once
#include <EVA/Assets/Asset.hpp>
#include <EVA/GFX/Mesh.hpp>

struct EVERSION(1) ModelDataMesh {
	String name;
	MeshData meshData;
};
EAUTO_SERIALIZE(ModelDataMesh);

struct EVERSION(1) ModelData {
	Vector<ModelDataMesh> meshes;
};
EAUTO_SERIALIZE(ModelData);

class Model : public Asset {
public:
	ECLASS_COMMON(Model)

	Vector<Mesh*> meshes;

	virtual Result LoadImpl(Deserializer& d) override;
};
