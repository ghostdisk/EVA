#pragma once
#include <EVA/Common.hpp>

enum AssetType
{
	AssetType_None = 0,
	AssetType_Mesh,
	AssetType_Texture,
	AssetType_Material,
};

struct Asset
{
	AssetType type     = AssetType_None;
	U32       id       = 0;
	char      name[64] = {};
};

void AssetsSkipToId(U32 id);

void   AssetInit(Asset* asset, AssetType type, const char* name);
Asset* AssetGet(U32 id, AssetType expected_type);
Asset* AssetGetByName(const char* name, AssetType expected_type);