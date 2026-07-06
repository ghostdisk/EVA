#include <EVA/Assets/Asset.hpp>


struct Material : Asset {
	Shader*  shader        = 0;
	Texture* color_texture = nullptr;
	float    texture_scale = 1.0f;
};