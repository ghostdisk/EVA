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
#include <EVA/Assets/ShaderSource.hpp>
#include <EVA/Assets/Map.hpp>
#include <EVA/Assets/EditorMap.hpp>
#include <stdio.h>
#include <mutex>

static std::mutex g_assetsLock = {};
static Vector<Asset*> g_assets = {};
static Vector<U32> free_ids = {};

static double g_lastScanTime = -100000;

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
	if (ext == ".shader")  return ShaderSource::StaticClass();
	if (ext == ".cshader") return Shader::StaticClass();
	if (ext == ".vs.glsl") return GLSLShader::StaticClass();
	if (ext == ".fs.glsl") return GLSLShader::StaticClass();
	return nullptr;
}

Asset* Asset::Get(String name, Type* type) {
	g_assetsLock.lock();
	DEFER(g_assetsLock.unlock());

	for (Asset* asset : g_assets) {
		if (asset->m_name == name)
			return asset;
	}

	Asset* asset = (Asset*)type->Instantiate(Allocator::HeapAllocator);
	asset->m_name = name.CopyToHeap();
	g_assets.push_back(asset);
	return asset;
}

bool Asset::AreAllInputsLoaded() {
	for (Asset* input : m_inputs)
		if (input->m_loadState != AssetLoadState::Loaded)
			return false;
	return true;
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

/*
Promise AssetsBuild() {
	ScratchArena scratch;

	Promise buildPromise = Promise::Create(0);
	String dir = scratch->Fmt("%s/Assets", EVA_BASE_DIR);

	FS::ReadDirectory(dir, &buildPromise, [](const FS::Stat& stat, void* ud) {
		Promise* signalPromise = (Promise*)ud;

		ScratchArena scratch;
		String ext = FS::GetExtension(stat.filename);
		String path_wo_ext = FS::WithoutExtension(stat.fullPath);

		if (ext == ".gltf" || ext == ".glb") {
			struct JobData {
				Promise signalPromise;
				ZTString fullPath;
				ZTString path_wo_ext;
			};
			JobData* jobData = new JobData {
				.signalPromise = *signalPromise,
				.fullPath = stat.fullPath.CopyToHeap(),
				.path_wo_ext = path_wo_ext.CopyToHeap(),
			};
			signalPromise->Block();

			Job job = Job::Create([](void* _jobData) {
				ZoneScopedN("Build asset (gltf)");

				ScratchArena scratch;
				JobData* jobData = (JobData*)_jobData;
				Model* model = new Model();
				BuildGLTF(model, jobData->fullPath);
				model->SaveToDisk(scratch->Fmt("%.*s.mdl", STRING_PRINTF_ARGS(jobData->path_wo_ext)));

				jobData->signalPromise.Signal();
				free(jobData->fullPath.data);
				free(jobData->path_wo_ext.data);
				delete jobData;
			}, jobData);
			job.Schedule();

		} else if (ext == ".shader") {
			ZoneScopedN("Build asset (shader)");

			struct JobData {
				Promise signalPromise;
				ZTString fullPath;
				ZTString path_wo_ext;
			};
			JobData* jobData = new JobData {
				.signalPromise = *signalPromise,
				.fullPath = stat.fullPath.CopyToHeap(),
				.path_wo_ext = path_wo_ext.CopyToHeap(),
			};
			signalPromise->Block();

			Job job = Job::Create([](void* _jobData) {
				ZoneScopedN("Build asset (shader)");

				ScratchArena scratch;
				JobData* jobData = (JobData*)_jobData;
				Result res = BuildShader(jobData->fullPath, scratch->Fmt("%.*s.cshader", STRING_PRINTF_ARGS(jobData->path_wo_ext)));
				if (!res) Fatal("Failed to build %s: %s", jobData->fullPath.c_str(), res.error->c_str());

				jobData->signalPromise.Signal();
				free(jobData->fullPath.data);
				free(jobData->path_wo_ext.data);
				delete jobData;
			}, jobData);
			job.Schedule();
		}
	});

	return buildPromise;
}
*/

void AssetLoad(Asset* asset, Promise signalPromise = {}) {
	printf("[Assets] loading %.*s\n", STRING_PRINTF_ARGS(asset->m_name));
	asset->m_loadState  = AssetLoadState::Loading;

	struct Userdata {
		Asset* asset;
		Promise signalPromise;
	};
	Userdata* data = new Userdata();
	data->asset = asset;
	data->signalPromise = signalPromise;

	Job job = Job::Create([](void* _data) {
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

		FILE* f = fopen(asset->m_fsPath, "rb");
		if (!f) {
			ConError(Err("[assets] failed to load %s: failed to open file", asset->m_name.c_str()));
			asset->m_loadState = AssetLoadState::Failed;
			return;
		}
		DEFER(fclose(f));

		Result res = asset->LoadImpl(f);
		if (res.error) {
			asset->m_loadState = AssetLoadState::Failed;
			ConError(Err("[assets] failed to load %s: %s", asset->m_name.c_str(), res.error->c_str()));
			return;
		}

		if (asset->HasMeta()) {
			FILE* metaFile = fopen(metaPath.c_str(), "wb");
			if (metaFile) {
				TextSerializer meta_serializer(metaFile);
				asset->SaveMetaImpl(meta_serializer);
				fclose(metaFile);
			}
		}

		asset->m_loadState = AssetLoadState::Loaded;

		for (Asset* output : asset->m_outputs) {
			if (output->AreAllInputsLoaded()) {
				AssetLoad(output);
			}
		}
	}, data);

	job.Schedule();
}

void AssetsScanDir(String mountpoint, String dir) {
	struct Userdata {
		String mountpoint;
	} userdata = { mountpoint };

	FS::ReadDirectory(dir, &userdata, [](const FS::Stat& stat, void* _data) {
		Userdata* data = (Userdata*)_data;
		ScratchArena scratch;

		ZTString virtualPath = scratch->Fmt("%.*s/%s", STRING_PRINTF_ARGS(data->mountpoint), stat.filename.c_str());

		if (stat.isDirectory) {
			AssetsScanDir(virtualPath, stat.fullPath);
		}
		else {
			Type* assetType = MapExtensionToType(FS::GetExtension(stat.filename));
			if (!assetType) return;

			Asset* asset = Asset::Get(virtualPath, assetType);
			if (!asset->m_fsPath) asset->m_fsPath = stat.fullPath.CopyToHeap();

			if (asset->m_loadTime < stat.mtime && (asset->m_loadState != AssetLoadState::Loading)) {
				asset->m_loadTime = stat.mtime;
				asset->m_loadState = AssetLoadState::Loading;

				AssetLoad(asset);
			}
		}
	});
}

void AssetsLoad() {
	ZoneScopedN("AssetsLoad");
}

void AssetsScanForChanges() {
	if (g_time - g_lastScanTime < 1) {
		return;
	}
	g_lastScanTime = g_time;

	ScratchArena scratch;

	String root1 = scratch->Fmt("%s/Assets", EVA_BASE_DIR);
	AssetsScanDir("", root1);
	String root2 = scratch->Fmt("%s/EVA/Shaders", EVA_BASE_DIR);
	AssetsScanDir("/Shaders", root2);
}

