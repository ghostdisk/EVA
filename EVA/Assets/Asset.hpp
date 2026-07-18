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


class Asset : public Object {
public:
	ECLASS_COMMON();

	FS::Timestamp    m_loadTime  = {};
	ZTString         m_name      = {};
	ZTString         m_fsPath    = {};
	Vector<Asset*>   m_inputs    = {};
	Vector<Asset*>   m_outputs   = {};
	AssetLoadState   m_loadState = AssetLoadState::Unloaded;

	/**
	 ** Get the pointer to an asset.
	 ** The asset's PRECISE type is required -- if does not yet exist, it will be created.
	 **/
	static Asset* Get(String name, Type* type);


	/**
	 ** Get the pointer to an asset.
	 ** The asset's PRECISE type is required -- if does not yet exist, it will be created.
	 **/
	template <typename T>
	static T* Get(String name) {
		Asset* assetBase =  Get(name, T::StaticClass());

		T* assetCast = dynamic_cast<T*>(assetBase);
		assert(assetCast); // if this fails, the same asset path is being created multiple times with different types

		return assetCast;
	}

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
	 **     FooAsset* input1 = Asset::Get<FooAsset>("/Foos/foo1.foo");
	 **     FooAsset* input2 = Asset::Get<FooAsset>("/Foos/foo2.foo");
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
	 ** LoadImpl is called when the asset is ready to load or build.
	 **/
	virtual Result LoadImpl(FILE* f) {
		return Success();
	}

	inline bool Loaded() {
		return m_loadState == AssetLoadState::Loaded;
	}
};

void AssetsScanForChanges();

void    AssetsLoad();
