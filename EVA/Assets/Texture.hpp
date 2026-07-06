#pragma once
#include <EVA/Assets/Asset.hpp>

struct ECLASS() Texture : Asset {
public:
ECLASS_COMMON();
	U32    handle = 0;
	size_t width  = 0;
	size_t height = 0;
};