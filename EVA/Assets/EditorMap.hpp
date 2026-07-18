#pragma once
#include <EVA/Assets/Asset.hpp>

class EditorMap : public Asset {
public:
	virtual bool AssetNameHasFileExtension() override {
		return true;
	}
	ECLASS_COMMON(EditorMap);
};
