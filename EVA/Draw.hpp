#pragma once
#include <EVA/Math.hpp>
#include <EVA/GL.hpp>
#include <vector>

struct Texture;
struct Sprite;
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
	int yoffset = 0;

	FontGlyph glyphs[256];
};

enum DrawQuadMode
{
	DrawQuadMode_SolidColor,
	DrawQuadMode_Text,
	DrawQuadMode_Sprite,
};

struct DrawQuad
{
	int mode;
	int pad0;
	int pad1;
	int pad2;
	float4 position_rect;
	float4 uv_rect;
	float4 tint;
};

struct DrawQuadRecord
{
	DrawQuadMode mode          = DrawQuadMode_SolidColor;
	Texture*     texture       = nullptr;
	float4       position_rect = {};
	float4       uv_rect       = {};
	float4       tint          = { 1,1,1,1 };
};

struct DrawContext
{
	std::vector<DrawQuadRecord> quads;
};

void DrawInitialize();
void DrawContextInit(DrawContext& dc);
void DrawRender(DrawContext& dc);

void DrawRectangle(DrawContext& dc, float4 color, int x, int y, int w, int h);
void DrawText(DrawContext& dc, Font* font, const char* text, int x, int y, float4 color);
void DrawSprite(DrawContext& dc, Sprite* sprite, int x, int y);
float MeasureText(Font* font, const char* text);

Font* FontLoad(const char* name, int size, int atlas_size);