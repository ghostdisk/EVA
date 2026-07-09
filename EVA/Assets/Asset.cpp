#include <EVA/Core/String.hpp>
#include <EVA/Core/Serialization.hpp>
#include <EVA/FS.hpp>
#include <EVA/Core/Arena.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/Console.hpp>
#include <EVA/Assets/Sprite.hpp>
#include <EVA/Assets/Font.hpp>
#include <EVA/Assets/Model.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Assets/Map.hpp>
#include <EVA/Assets/EditorMap.hpp>
#include <stdio.h>
#include <vector>

static std::vector<Asset*> assets_old = { nullptr };
static std::vector<Asset*> g_assets = { };
static std::vector<U32> free_ids = {};

void AssetsSkipToId(U32 id) {
	if (assets_old.size() > id) Fatal("AssetsSkipToId: we're already past that.");
	assets_old.resize(id);
}

Asset* Asset::GetImpl(String name) {
	for (Asset* asset : g_assets) {
		if (asset->name_new == name)
			return asset;
	}
	return nullptr;
}

Type* MapExtensionToType(String ext) {
	if (ext == ".ttf")     return Font::StaticClass();
	// if (ext == ".glb")     return GLTF::StaticClass();
	if (ext == ".png")     return Texture::StaticClass();
	if (ext == ".mdl")     return Model::StaticClass();
	if (ext == ".jpg")     return Texture::StaticClass();
	if (ext == ".jpeg")    return Texture::StaticClass();
	if (ext == ".psd")     return Texture::StaticClass();
	if (ext == ".map")     return Map::StaticClass();
	if (ext == ".mpe")     return EditorMap::StaticClass();
	return nullptr;
}

void BuildAssets(String dir) {
	FS::ReadDirectory(dir, nullptr, [](const FS::Stat& stat, void* ud) {
		String ext = FS::GetExtension(stat.filename);
		if (ext == ".gltf" || ext == ".glb") {
			Model* model = new Model();
			BuildGLTF(model, stat.full_path);
			String path_wo_ext = FS::WithoutExtension(stat.full_path);
			model->SaveToDisk(Fmt(FrameArena, "%.*s.mdl", STRING_PRINTF_ARGS(path_wo_ext)));
		}
	});
}

void LoadAssetsFromDir(String dir) {
	FS::ReadDirectory(dir, nullptr, [](const FS::Stat& stat, void* ud) {
		Type* asset_type = MapExtensionToType(FS::GetExtension(stat.filename));
		if (!asset_type) return;

		Asset* asset = (Asset*)asset_type->Instantiate();
		g_assets.push_back(asset);
		asset->name_new = FS::WithoutExtension(stat.filename).CopyToHeap();

		FILE* f = fopen(stat.full_path.c_str(), "rb");
		if (!f) {
			printf("failed to read file %.*s", STRING_PRINTF_ARGS(stat.full_path));
			asset->state = AssetLoadState::Failed;
			return;
		}
		DEFER(fclose(f));

		ZTString meta_path = Fmt(FrameArena, "%.*s.meta", STRING_PRINTF_ARGS(stat.full_path));
		void* meta = nullptr;
		size_t meta_size = 0;

		FILE* meta_file = fopen(meta_path.c_str(), "rb");
		if (meta_file) {
			TextDeserializer d(meta_file);
			asset->LoadMetaImpl(d);
			fclose(meta_file);
		}

		asset->state = AssetLoadState::Loading;
		Result res = asset->LoadImpl(f);
		if (!res) {
			asset->state = AssetLoadState::Failed;
			ConError(res);
			return;
		}
		asset->state = AssetLoadState::Loaded;
		printf("%.*s -> %p\n", STRING_PRINTF_ARGS(asset->name_new), asset);

		meta_file = fopen(meta_path.c_str(), "wb");
		if (meta_file) {
			TextSerializer meta_serializer(meta_file);
			asset->SaveMetaImpl(meta_serializer);
			fclose(meta_file);
		}

	});
}

void AssetsLoad() {
	String root = Fmt(FrameArena, "%s/Assets", EVA_BASE_DIR);
	 BuildAssets(root);
	LoadAssetsFromDir(root);
}

void AssetInit(Asset* asset, const char* name) {
	snprintf(asset->name, sizeof(asset->name), "%s", name);

	if (free_ids.size()) {
		asset->id = free_ids.back();
		free_ids.pop_back();
		assets_old[asset->id] = asset;
	} else {
		asset->id = assets_old.size();
		assets_old.push_back(asset);
	}
}

Asset* AssetGet(U32 id, Type* expected_type) {
	if (id < assets_old.size()) {
		Asset* asset = assets_old[id];
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
	//       in the assets_old vector.
	for (Asset* asset : assets_old) {
		if (asset && strcmp(asset->name, name) == 0) {
			if (asset->GetClass() == expected_type)
				return asset;
		}
	}
	return nullptr;
}

void AssetDeinit(Asset* asset) {
	free_ids.push_back(asset->id);
	assets_old[asset->id] = nullptr;
}
