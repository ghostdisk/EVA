#pragma once
#include <EVA/Assets/Asset.hpp>

class ECLASS() Texture : public Asset {
public:
ECLASS_COMMON();
	U32    handle = 0;
	size_t width  = 0;
	size_t height = 0;
};