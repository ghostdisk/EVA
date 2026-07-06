#include <EVA/String.hpp>
#include <EVA/FS.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/Assets/Sprite.hpp>
#include <EVA/Assets/Font.hpp>
#include <EVA/Assets/GLTF.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Assets/Map.hpp>
#include <EVA/Assets/EditorMap.hpp>
#include <stdio.h>
#include <vector>

static std::vector<Asset*> assets = { nullptr };
static std::vector<U32> free_ids = {};

void AssetsSkipToId(U32 id) {
	if (assets.size() > id) Fatal("AssetsSkipToId: we're already past that.");
	assets.resize(id);
}

Type* MapExtensionToType(String ext) {
	if (ext == ".ttf")     return Font::StaticClass();
	if (ext == ".glb")     return GLTF::StaticClass();
	if (ext == ".png")     return Texture::StaticClass();
	if (ext == ".jpg")     return Texture::StaticClass();
	if (ext == ".jpeg")    return Texture::StaticClass();
	if (ext == ".psd")     return Texture::StaticClass();
	if (ext == ".map")     return Map::StaticClass();
	if (ext == ".mpe")     return EditorMap::StaticClass();
	return nullptr;
}

void LoadAssetsRecursive(String dir) {
	FS::ReadDirectory(dir, nullptr, [](const FS::Stat& stat, void* ud) {
		Type* asset_type = MapExtensionToType(FS::GetExtension(stat.filename));
		if (!asset_type) return;

		Asset* asset = (Asset*)asset_type->Instantiate();
		// TODO: Do something with this :-)
		printf("%.*s -> %p", STRING_PRINTF_ARGS(stat.filename), asset);
	});
}

void AssetsInitialize() {
	LoadAssetsRecursive(Fmt(FrameArena, "%s/Assets", EVA_BASE_DIR));
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

Asset* AssetGet(U32 id, Type* expected_type) {
	if (id < assets.size()) {
		Asset* asset = assets[id];
		if (asset->GetClass() == expected_type) {
			return asset;
		} else {
			return nullptr;
		}
	} else {
		return nullptr;
	}
}

Asset* AssetGetByName(const char* name, Type* expected_type) {
	// TODO: This is even slower than what you may expect looking at this code, as there's a bunch of holes
	//       in the assets vector.
	for (Asset* asset : assets) {
		if (asset && strcmp(asset->name, name) == 0) {
			if (asset->GetClass() == expected_type)
				return asset;
		}
	}
	return nullptr;
}

void AssetDeinit(Asset* asset) {
	free_ids.push_back(asset->id);
	assets[asset->id] = nullptr;
}
