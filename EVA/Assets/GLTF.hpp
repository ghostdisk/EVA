#pragma once
#include <EVA/Assets/Asset.hpp>

/**
 ** GLTF
 **/
class GLTF : public Asset {
public:
	ECLASS_COMMON(GLTF);

	virtual AssetLoadType GetLoadType() override {
		return AssetLoadType::File;
	}

	virtual bool AssetNameHasFileExtension() override {
		return true;
	}

	virtual Result LoadImpl(Deserializer& d) override;
};
