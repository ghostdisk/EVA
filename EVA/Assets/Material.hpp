#pragma once
#include <EVA/Assets/Asset.hpp>

class Shader;
class Texture;

class ECLASS() Material : public Asset {
public:
ECLASS_COMMON()

	Shader*  shader        = 0;
	Texture* color_texture = nullptr;
	float    texture_scale = 1.0f;
};