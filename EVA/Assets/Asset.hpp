#pragma once
#include <EVA/Object.hpp>

typedef struct FT_FaceRec_*  FT_Face;
struct Sprite;
struct Mesh;
struct Texture;
struct Material;
struct Texture;
struct CSGBrush;
struct Shader;

enum AssetType {
	AssetType_None = 0,
	AssetType_Mesh,
	AssetType_Texture,
	AssetType_Material,
	AssetType_Sprite,
	AssetType_Font,
	AssetType_Shader,
};

class ECLASS() Asset : Object {
public:
	ECLASS_COMMON();

	AssetType type     = AssetType_None;
	U32       id       = 0;
	char      name[64] = {};
};

void    AssetInit(Asset* asset, AssetType type, const char* name);
void    AssetDeinit(Asset* asset);
Asset*  AssetGet(U32 id, AssetType expected_type);
Asset*  AssetGetByName(const char* name, AssetType expected_type);
void    AssetsSkipToId(U32 id);
