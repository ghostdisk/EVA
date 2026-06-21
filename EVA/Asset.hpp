#pragma once
#include <EVA/Common.hpp>

struct Texture;
struct Material;
struct Collider;

enum AssetType
{
	AssetType_None = 0,
	AssetType_Mesh,
	AssetType_Texture,
	AssetType_Material,
	AssetType_Sprite,
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
	Texture* color_texture = nullptr;
};

void   AssetInit(Asset* asset, AssetType type, const char* name);
void   AssetDeinit(Asset* asset);
Asset* AssetGet(U32 id, AssetType expected_type);
Asset* AssetGetByName(const char* name, AssetType expected_type);
void   AssetsSkipToId(U32 id);

Sprite* SpriteCreate(const char* name, Texture* texture, int x, int y, int w, int h);