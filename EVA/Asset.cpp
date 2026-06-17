#include <EVA/Asset.hpp>
#include <stdio.h>
#include <vector>

static std::vector<Asset*> assets = { 0 };

void AssetsSkipToId(U32 id)
{
	if (assets.size() > id)
	{
		Fatal("AssetsSkipToId: we're already past that.");
	}
	assets.resize(id);
}

void AssetInit(Asset* asset, AssetType type, const char* name)
{
	asset->type = type;

	snprintf(asset->name, sizeof(asset->name), "%s", name);

	asset->id = assets.size();
	assets.push_back(asset);
}

Asset* AssetGet(U32 id)
{
	return assets[id];
}