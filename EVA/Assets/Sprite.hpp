#include <EVA/Assets/Asset.hpp>

struct Sprite : Asset {
	Texture* texture;
	int x, y, w, h;
};

Sprite* SpriteCreate(const char* name, Texture* texture, int x, int y, int w, int h);