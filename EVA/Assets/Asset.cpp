#include <EVA/Core/Basic.hpp>
#include <EVA/Core/Serialization.hpp>
#include <EVA/Core/FS.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Console.hpp>
#include <EVA/Assets/Sprite.hpp>
#include <EVA/Assets/Font.hpp>
#include <EVA/Assets/Model.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Assets/Shader.hpp>
#include <EVA/Assets/Map.hpp>
#include <EVA/Assets/EditorMap.hpp>
#include <EVA/Assets/GLTF.hpp>
#include <stdio.h>
#include <mutex>

DirectoryAsset* g_rootAsset = {};
static std::mutex g_assetsLock = {};
static Vector<U32> free_ids = {};

struct AssetClassRegistration {
	String extension = {};
	Type*  assetType = nullptr;
};
static Vector<AssetClassRegistration> g_assetClassRegistrations = {};

static double g_lastScanTime = -100000;

void AssetsInitialize() {
	ScratchArena scratch;

	for (Type* assetType : Asset::StaticClass()->subclasses) {
		Asset* cdo = (Asset*)assetType->defaultObject;
		if (!cdo) continue;

		for (String extension : cdo->GetFileExtensions()) {
			g_assetClassRegistrations.push_back(AssetClassRegistration{
				.extension = extension,
				.assetType = assetType,
			});
		}
	}

	g_rootAsset = new DirectoryAsset();
	g_rootAsset->m_name = "/";
	g_rootAsset->m_fsPath = scratch->Fmt("%s/Assets", EVA_BASE_DIR).CopyToHeap();
	g_rootAsset->m_backing = AssetBacking::Directory;
}

static Type* MapExtensionToFileType(String extension) {
	for (const AssetClassRegistration& registration : g_assetClassRegistrations)
		if (registration.extension == extension) return registration.assetType;
	return nullptr;
}

Asset* Asset::ResolveImpl(String path) {
	Asset* asset = this;

	while (path.size) {
		String name, rem;
		path.Cut('/', &name, &rem);
		path = rem;
		asset = asset->GetExistingChild(name);
		if (!asset) return asset;
	}

	return asset;
}

Asset* Asset::GetImpl(String absolutePath) {
	assert(absolutePath.size > 0 && absolutePath[0] == '/');
	return g_rootAsset->ResolveImpl(absolutePath.Skip(1));
}

Asset* Asset::GetExistingChild(String name) {
	for (Asset* child : m_children)
		if (child->m_name == name)
			return child;
	return nullptr;
}

Asset* Asset::GetOrCreateChild(String name, Type* assetClass) {
	Asset* child = GetExistingChild(name);
	if (child) {
		Type* childClass = child->GetClass();
		if (childClass == assetClass) {
			return child;
		} else {
			ConError(Err("Asset::Get - requested %.*s with type %s, but it's a %s",
				STRING_PRINTF_ARGS(name), assetClass->name.c_str(), assetClass->name.c_str()));
			return nullptr;
		}
	}

	child = (Asset*)assetClass->Instantiate(Allocator::HeapAllocator);
	child->m_name = name.CopyToHeap();
	m_children.push_back(child);
	return child;
}

void Asset::AddInput(Asset* input) {
	g_assetsLock.lock();
	DEFER(g_assetsLock.unlock());

	assert(m_loadState == AssetLoadState::Loading); // AddInput should only be called while initially loading the asset

	for (Asset* existingInput : m_inputs)
		if (input == existingInput)
			return;

	input->m_outputs.push_back(this);
	m_inputs.push_back(input);
}

bool FileAssetDirty(Asset* asset) {
	assert(asset->m_backing == AssetBacking::File);

	if (asset->m_loadTime == asset->m_timeOnDisk) return false;
	if (asset->m_loadState == AssetLoadState::Unloaded || asset->m_loadState == AssetLoadState::Loading) return false;
	if (!asset->AreAllInputsLoaded()) return false;
	
	return true;
}

static void LoadFileAsset(Asset* asset, Promise signalPromise = {}) {
	if (asset->m_backing != AssetBacking::File) {
		assert(0);
		return;
	}
	if (!FileAssetDirty(asset) && asset->m_loadState != AssetLoadState::Unloaded) return;

	printf("[Assets] loading %.*s\n", STRING_PRINTF_ARGS(asset->m_name));

	asset->m_loadTime = asset->m_timeOnDisk;
	asset->m_loadState  = AssetLoadState::Loading;
	asset->m_loadState = AssetLoadState::Loading;

	struct Userdata {
		Asset* asset;
		Promise signalPromise;
	};
	Userdata* data = new Userdata();
	data->asset = asset;
	data->signalPromise = signalPromise;

	Job job = Job::Create([](void* _data) {
		ZoneScopedN("LoadFileAsset job");

		Userdata* data = (Userdata*)_data;
		Asset* asset = data->asset;
		ScratchArena scratch;

		DEFER({
			data->signalPromise.Signal();
			delete data;
		});

		ZTString metaPath = scratch->Fmt("%s.meta", asset->m_fsPath.c_str());

		if (asset->HasMeta()) {
			void* meta = nullptr;
			size_t meta_size = 0;
			FILE* meta_file = fopen(metaPath.c_str(), "rb");
			if (meta_file) {
				TextDeserializer d(meta_file);
				asset->LoadMetaImpl(d);
				fclose(meta_file);
				if (d.res.error) {
					// failing to load meta is generally not fatal, report and continue.
					ConError(Err("[assets] error loading %s meta: %s", asset->m_name.c_str(), d.res.error->c_str()));
				}
			}
		}

		Result res;
		if (asset->GetLoadType() == AssetLoadType::Deserializer) {
			FILE* f = fopen(asset->m_fsPath, "rb");
			if (!f) {
				ConError(Err("[assets] failed to load %s: failed to open file", asset->m_name.c_str()));
				asset->m_loadState = AssetLoadState::Failed;
				return;
			}
			TextDeserializer deserializer(f, scratch);
			res = asset->LoadImpl(deserializer);
			fclose(f);
		} else {
			TextDeserializer d(nullptr);
			d.SetError(Err("this deserializer is a dummy and should not be used."));
			res = asset->LoadImpl(d);
		}

		if (res.error) {
			asset->m_loadState = AssetLoadState::Failed;
			ConError(Err("[assets] failed to load %s: %s", asset->m_name.c_str(), res.error->c_str()));
			return;
		}
		else if (!asset->AreAllInputsLoaded()) {
 			asset->m_loadState = AssetLoadState::WaitingForInputs;
			return;
		}
		else {
			asset->m_loadState =  AssetLoadState::Loaded;
			if (asset->HasMeta()) {
				FILE* metaFile = fopen(metaPath.c_str(), "wb");
				if (metaFile) {
					TextSerializer meta_serializer(metaFile);
					asset->SaveMetaImpl(meta_serializer);
					fclose(metaFile);
				}
			}

			// This implements hot-reloading via a Makefile-like graph - if we have file-backed assets that depend on
			// this, reload them after this one loads.
			for (Asset* output : asset->m_outputs) {
				if (output->AreAllInputsLoaded() && output->m_backing == AssetBacking::File &&
					(output->m_loadState == AssetLoadState::Loaded || output->m_loadState == AssetLoadState::Failed)) {
					LoadFileAsset(output);
				}
			}
		}
	}, data);

	job.Schedule();
}

void DirectoryAsset::ScanForChanges() {
	struct Userdata {
		DirectoryAsset* directory;
	} userdata = { this };

	FS::ReadDirectory(m_fsPath, &userdata, [](const FS::Stat& stat, void* _data) {
		Userdata* data = (Userdata*)_data;
		ScratchArena scratch;
		DirectoryAsset* directory = data->directory;

		if (stat.isDirectory) {
			DirectoryAsset* childDirectory = (DirectoryAsset*)directory->GetOrCreateChild(stat.filename, DirectoryAsset::StaticClass());
			if (!childDirectory->m_fsPath.size) {
				childDirectory->m_fsPath = stat.fullPath.CopyToHeap();
				childDirectory->m_backing = AssetBacking::Directory;
			}

			childDirectory->ScanForChanges();
		}
		else {
			Type* assetType = MapExtensionToFileType(FS::GetExtension(stat.filename));
			if (assetType) {
				String name = stat.filename;

				Asset* cdo = (Asset*)assetType->defaultObject;
				if (!cdo->AssetNameHasFileExtension()) {
					name = FS::WithoutExtension(name);
				}

				Asset* asset = directory->GetOrCreateChild(name, assetType);
				if (asset) {
					if (!asset->m_fsPath) {
						asset->m_fsPath = stat.fullPath.CopyToHeap();
						asset->m_backing = AssetBacking::File;
					}
					asset->m_timeOnDisk = stat.mtime;
					if (FileAssetDirty(asset)) {
						LoadFileAsset(asset);
					}
				}
			}
		}
	});
}

void Asset::LoadRecursive() {
	if (m_backing == AssetBacking::File && (m_loadState == AssetLoadState::Unloaded || FileAssetDirty(this))) {
		LoadFileAsset(this);
	}
	for (Asset* child : m_children) {
		child->LoadRecursive();
	}
}

void AssetsScanForChanges() {
	if (g_time - g_lastScanTime < 1) {
		return;
	}
	g_lastScanTime = g_time;
	g_rootAsset->ScanForChanges();
}

