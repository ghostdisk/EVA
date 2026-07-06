#pragma once
#include <EVA/Assets/Asset.hpp>

struct Texture;

struct ECLASS() Sprite : Asset {
public:
	ECLASS_COMMON();

	Texture* texture;
	int x, y, w, h;
};

Sprite* SpriteCreate(const char* name, Texture* texture, int x, int y, int w, int h);