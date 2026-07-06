#include <EVA/Assets/Sprite.hpp>

Sprite* SpriteCreate(const char* name, Texture* texture, int x, int y, int w, int h) {
	Sprite* sprite = new Sprite();
	AssetInit(sprite, AssetType_Sprite, name);
	sprite->texture = texture;
	sprite->x = x;
	sprite->y = y;
	sprite->w = w;
	sprite->h = h;
	return sprite;
}