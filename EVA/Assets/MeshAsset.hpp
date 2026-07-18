#pragma once
#include <EVA/Assets/Asset.hpp>

class Mesh;

class MeshAsset : public Asset {
public:
	ECLASS_COMMON();

	Mesh* m_mesh = nullptr;

	virtual Result LoadImpl(Deserializer& d) override;
};