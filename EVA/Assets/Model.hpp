#pragma once
#include <EVA/Assets/Asset.hpp>

class Mesh;

class Model : public Asset {
public:
	ECLASS_COMMON()

	Vector<Mesh*> meshes;

	virtual Result LoadImpl(FILE* f) override;

	Result SaveToDisk(ZTString path);
};

Result BuildGLTF(Model* model, ZTString path);

void Serialize(Serializer& s, Model* model);
void Deserialize(Deserializer& d, Model* model);
