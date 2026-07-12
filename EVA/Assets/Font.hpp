#pragma once
#include <EVA/Assets/Asset.hpp>

typedef struct FT_FaceRec_*  FT_Face;
class Texture;

struct FontGlyph {
	int x       = 0;
	int y       = 0;
	int width   = 0;
	int height  = 0;
	int advance = 0;
	int xoffs   = 0;
	int yoffs   = 0;
};

class Font : public Asset {
public:
	ECLASS_COMMON();
	FT_Face  face        = {};
	Texture* atlas       = 0;
	int      pixel_size  = 0;
	int      line_height = 0;

	FontGlyph glyphs[256];
};

void FontInitialize();
Font* FontLoad(const char* name, int size, int atlas_size);
