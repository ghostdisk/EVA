#include <EVA/Assets/Asset.hpp>
#include <EVA/Assets/Sprite.hpp>
#include <stdio.h>
#include <vector>

static std::vector<Asset*> assets = { nullptr };
static std::vector<U32> free_ids = {};

void AssetsSkipToId(U32 id) {
	if (assets.size() > id) Fatal("AssetsSkipToId: we're already past that.");
	assets.resize(id);
}

void AssetInit(Asset* asset, const char* name) {
	snprintf(asset->name, sizeof(asset->name), "%s", name);

	if (free_ids.size()) {
		asset->id = free_ids.back();
		free_ids.pop_back();
		assets[asset->id] = asset;
	} else {
		asset->id = assets.size();
		assets.push_back(asset);
	}
}

Asset* AssetGet(U32 id, const Type& expected_type) {
	if (id < assets.size()) {
		Asset* asset = assets[id];
		if (&asset->GetType() == &expected_type) {
			return asset;
		} else {
			return nullptr;
		}
	} else {
		return nullptr;
	}
}

Asset* AssetGetByName(const char* name, const Type& expected_type) {
	// TODO: This is even slower than what you may expect looking at this code, as there's a bunch of holes
	//       in the assets vector.
	for (Asset* asset : assets) {
		if (asset && strcmp(asset->name, name) == 0) {
			if (&asset->GetType() == &expected_type)
				return asset;
		}
	}
	return nullptr;
}

void AssetDeinit(Asset* asset) {
	free_ids.push_back(asset->id);
	assets[asset->id] = nullptr;
}
