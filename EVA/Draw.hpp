#pragma once
#include <EVA/Math.hpp>
#include <EVA/GL.hpp>
#include <vector>


struct Texture;
typedef struct FT_FaceRec_*  FT_Face;

struct FontGlyph
{
	int x       = 0;
	int y       = 0;
	int width   = 0;
	int height  = 0;
	int advance = 0;
	int xoffs   = 0;
	int yoffs   = 0;
};

struct Font
{
	FT_Face face = {};
	Texture* atlas = 0;

	FontGlyph glyphs[256];
};
struct DrawQuad
{
	float4 position_rect;
	float4 uv_rect;
};
struct DrawQuadRecord
{
	Texture* texture;
	float4 position_rect;
	float4 uv_rect;
};

struct DrawContext
{
	std::vector<DrawQuadRecord> quads;
};

void DrawInitialize();
void DrawContextInit(DrawContext& dc);
void DrawRender(DrawContext& dc);

void DrawText(DrawContext& dc, Font* font, const char* text, int x, int y);

Font* FontLoad(const char* name, int size, int atlas_size);