#pragma once
#include <EVA/Assets/Asset.hpp>

class Map : public Asset {
public:
	ECLASS_COMMON(Map);

	virtual Vector<String> GetFileExtensions() override {
		return { ".map" };
	}
};
