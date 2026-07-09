#pragma once
#include <EVA/Assets/Asset.hpp>
#include <vector>

class Mesh;

class ECLASS() Model : public Asset {
public:
	ECLASS_COMMON()

	std::vector<Mesh*> meshes;

	virtual Result LoadImpl(FILE* f) override;

	Result SaveToDisk(ZTString path);
};

Result BuildGLTF(Model* model, ZTString path);

void Serialize(Serializer& s, Model* model);
void Deserialize(Deserializer& s, Model* model);