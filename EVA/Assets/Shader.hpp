#pragma once
#include <EVA/Assets/Asset.hpp>

class Shader : public Asset {
public:
	ECLASS_COMMON();

	U32 handle;
};
