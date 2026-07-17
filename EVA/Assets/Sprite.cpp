#include <EVA/Assets/Sprite.hpp>

Sprite* SpriteCreate(const char* name, Texture* texture, int x, int y, int w, int h) {
	printf("[asset] SpriteCreate uses old style\n");
	Sprite* sprite = new Sprite();
	sprite->texture = texture;
	sprite->x = x;
	sprite->y = y;
	sprite->w = w;
	sprite->h = h;
	return sprite;
}