#pragma once
#include <EVA/Assets/Asset.hpp>

/**
 ** GLTF
 **/
class GLTF : public Asset {
public:
	ECLASS_COMMON();

	virtual AssetLoadType GetLoadType() override {
		return AssetLoadType::File;
	}

	virtual Result LoadImpl(Deserializer& d) override;
};
