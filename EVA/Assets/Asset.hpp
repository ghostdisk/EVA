#pragma once
#include <EVA/Common.hpp>

typedef struct FT_FaceRec_*  FT_Face;
struct Texture;
struct Material;
struct Texture;
struct CSGBrush;
struct Shader;

enum AssetType
{
	AssetType_None = 0,
	AssetType_Mesh,
	AssetType_Texture,
	AssetType_Material,
	AssetType_Sprite,
	AssetType_Font,
	AssetType_Map,
	AssetType_Shader,
};

struct Asset
{
	AssetType type     = AssetType_None;
	U32       id       = 0;
	char      name[64] = {};
};

struct Sprite : Asset
{
	Texture* texture;
	int x, y, w, h;
};

struct Mesh : Asset
{
	U32           vao             = 0;
	U32           vbo             = 0;
	U32           ibo             = 0;
	U32           index_count     = 0;
	U32           vertex_count    = 0;
	Material*     default_maerial = nullptr;
};

struct Texture : Asset
{
	U32    handle = 0;
	size_t width  = 0;
	size_t height = 0;
};

struct Material : Asset
{
	Shader*  shader        = 0;
	Texture* color_texture = nullptr;
};

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

struct Font : Asset
{
	FT_Face  face        = {};
	Texture* atlas       = 0;
	int      pixel_size  = 0;
	int      line_height = 0;

	FontGlyph glyphs[256];
};

struct Map : Asset
{
	CSGBrush* brushes;
};

struct Shader : Asset
{
	U32 handle;
};

void    AssetInit(Asset* asset, AssetType type, const char* name);
void    AssetDeinit(Asset* asset);
Asset*  AssetGet(U32 id, AssetType expected_type);
Asset*  AssetGetByName(const char* name, AssetType expected_type);
void    AssetsSkipToId(U32 id);

Sprite* SpriteCreate(const char* name, Texture* texture, int x, int y, int w, int h);