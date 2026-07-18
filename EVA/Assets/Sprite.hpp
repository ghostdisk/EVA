#pragma once
#include <EVA/Assets/Asset.hpp>

class Texture;

class Sprite : public Asset {
public:
	ECLASS_COMMON(Sprite);

	Texture* texture;
	int x, y, w, h;
};

Sprite* SpriteCreate(const char* name, Texture* texture, int x, int y, int w, int h);
