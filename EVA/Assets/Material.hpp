#pragma once
#include <EVA/Assets/Asset.hpp>


struct ECLASS() Material : Asset {
public:
ECLASS_COMMON()

	Shader*  shader        = 0;
	Texture* color_texture = nullptr;
	float    texture_scale = 1.0f;
};