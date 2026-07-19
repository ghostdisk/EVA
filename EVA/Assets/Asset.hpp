#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Core/FS.hpp>
#include <EVA/Async/JobSystem.hpp>

#ifndef _FILE_DEFINED
    #define _FILE_DEFINED
    typedef struct _iobuf
    {
        void* _Placeholder;
    } FILE;
#endif

enum class AssetLoadState {
	Unloaded = 0,
	Loading,
	Loaded,
	Failed,
	WaitingForInputs,
};

/**
 ** AssetLoadType - specify how the asset should be loaded
 **/
enum class AssetLoadType {
	// Deserializer mode means a valid Deserializer is passed to LoadImpl, and we load the asset off that.
	// This is used for all our custom formats.
	Deserializer,

	// File mode means that you'll manually load the file from disk (this asset type does not use our serialization
	// system). This is used for filetypes which we don't control - png, gtlf, ...
	File,
};

enum class AssetBacking {
	None,
	// The asset is backed by a file. The asset system will track changes to this file and reload it on demand
	File,
	// The asset is a directory.
	Directory,
	// The asset is a subasset of a File-asset. It's the parent File asset's responsibility to load/unload this.
	Subasset,
};

class Asset : public Object {
public:
	ECLASS_COMMON(Asset);

	AssetBacking     m_backing                  = AssetBacking::None;
	FS::Timestamp    m_loadTime                 = {};
	FS::Timestamp    m_timeOnDisk               = {};
	ZTString         m_name                     = {};
	ZTString         m_fsPath                   = {};
	Vector<Asset*>   m_children                 = {};
	AssetLoadState   m_loadState                = AssetLoadState::Unloaded;
	Vector<Asset*>   m_inputs                   = {};
	Vector<Asset*>   m_outputs                  = {};

	/**
	 ** Does this asset type have a corresponding .meta file? This is useful for assets like textures, where we don't
	 ** have control over the type of the input data (e.g. a png file that we need to store extra data for)
	 **/
	virtual bool HasMeta() { return false; }

	/**
	 ** LoadMetaImpl is called before the asset is loaded, if it has a meta file.
	 ** If this is the first time this asset is being loaded, there will be no meta file yet, so provide sane defaults
	 ** for whatever data you wish to store in the meta file.
	 **/
	virtual void LoadMetaImpl(Deserializer& deserializer) {}

	/**
	 ** SaveMetaImpl is called after an asset is successfully loaded.
	 **/
	virtual void SaveMetaImpl(Serializer& serializer) {}

	/**
	 ** @TODO: This is currently ignored.
	 **
	 ** If true, the FS path Assets/Foos/asset.foo will correspond to "/Foos/asset.foo"
	 ** If false, the FS path Assets/Foos/asset.foo will correspond to "/Foos/asset"
	 **
	 ** For most assets this is false - e.g. we do "tex_whatever", not "tex_whatever.png"
	 ** For some assets this is required - e.g. this is used to disambiguate the source and built shaders
	 **/
	virtual bool AssetNameHasFileExtension() {
		return false;
	}

	/**
	 ** GetLoadType - See comments on AssetLoadType
	 **/
	virtual AssetLoadType GetLoadType() {
		return AssetLoadType::Deserializer;
	}

	/**
	 ** LoadImpl is called when the asset is ready to load or build.
	 **/
	virtual Result LoadImpl(Deserializer& d) {
		return Success();
	}

	/**
	 ** What file extensions correspond to this Asset class?
	 **
	 ** These are walked once at program launch for each asset subclass to register them.
	 **/
	virtual Vector<String> GetFileExtensions() {
		return {};
	}

	/**
	 ** Type-safe wrapper around GetImpl.
	 ** Get an asset relative to the root asset. absolutePath must start with / - e.g. "/Textures/tex.png"
	 **/
	template <typename T>
	static T* Get(String path) {
		Asset* assetBase = GetImpl(path);
		T* assetCast = dynamic_cast<T*>(assetBase);
		return assetCast;
	}

	/**
	 ** Type-safe wrapper around ResolveImpl.
	 ** Resolve a child asset.
	 ** Path can either be a direct child name, or a slash-separated path, e.g. "DirectChild/GrandChild/GrandGrandChild.png"
	 **/
	template <typename T>
	T* Resolve(String path) {
		Asset* assetBase = ResolveImpl(path);
		T* assetCast = dynamic_cast<T*>(assetBase);
		return assetCast;
	}

	/**
	 ** Search for a direct child asset by name. No path resolution done.
	 **/
	Asset* GetExistingChild(String name);

	/**
	 ** Search for a direct child asset by name, and if it's missing, create it.
	 **/
	Asset* GetOrCreateChild(String name, Type* type);

	/**
	 ** Returns true if all registered inputs are fully loaded, which means this asset is ready to load as well.
	 **/
	bool AreAllInputsLoaded() {
		for (Asset* input : m_inputs)
			if (!input->Loaded())
				return false;
		return true;
	}

	/**
	 ** AddInput registers another asset as an input (dependency) to this one.
	 ** The asset system will ensure that dependencies are loaded successfully before the current asset is attempted
	 ** to be loaded.
	 ** 
	 ** Call this during LoadInput like so:
	 ** 
	 ** Result MyAssetType::LoadImpl(FILE* f) {
	 **     FooAsset* input1 = Asset::Get<FooAsset>("/Foos/foo1");
	 **     FooAsset* input2 = Asset::Get<FooAsset>("/Foos/foo2");
	 ** 	AddInput(input1);
	 ** 	AddInput(input2);
	 ** 	if (!AreAllInputsLoaded()) return Success(); // don't error out if the reason we can't load is unloaded inputs
	 **     ... your actual load logic
	 ** }
	 **
	 ** Call this during LoadImpl. You can call it directly with the result of Asset::Get().
	 ** During the initial of LoadImpl the asset system does not yet know this asset's dependencies, so just call this
	 ** a few times and exit early, e.g.
	 **/
	void AddInput(Asset* input);

	/**
	 **
	 **/
	void LoadRecursive();

	inline bool Loaded() {
		return m_loadState == AssetLoadState::Loaded;
	}


private:
	/**
	 ** Resolve a child asset.
	 ** Path can either be a direct child name, or a slash-separated path, e.g. "DirectChild/GrandChild/GrandGrandChild.png"
	 **/
	Asset* ResolveImpl(String path);

	/**
	 ** Get an asset relative to the root asset. absolutePath must start with / - e.g. "/Textures/tex.png"
	 **/
	static Asset* GetImpl(String absolutePath);
};

class DirectoryAsset : public Asset {
public:
	ECLASS_COMMON(DirectoryAsset);

	void ScanForChanges();
};

void AssetsInitialize();
void AssetsScanForChanges();

extern DirectoryAsset* g_rootAsset;