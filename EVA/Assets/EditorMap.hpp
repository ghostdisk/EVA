#pragma once
#include <EVA/Assets/Asset.hpp>

class EditorMap : public Asset {
public:
	virtual bool AssetNameHasFileExtension() override {
		return true;
	}
	virtual Vector<String> GetFileExtensions() override {
		return { ".mpe" };
	}
	ECLASS_COMMON(EditorMap);
};
