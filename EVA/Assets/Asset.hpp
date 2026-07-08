#pragma once
#include <EVA/Core/String.hpp>
#include <EVA/Core/Object.hpp>
#include <EVA/Core/Result.hpp>

enum class AssetLoadState {
	Unloaded = 0,
	Loading,
	Loaded,
	Failed,
};

class ECLASS() Asset : public Object {

public:
	ECLASS_COMMON();

	AssetLoadState state       = AssetLoadState::Unloaded;
    U32            id          = 0;
    char           name[64]    = {};
	ZTString       name_new    = {};

	virtual void LoadMetaImpl(Deserializer& deserializer) {
	}

	virtual void SaveMetaImpl(Serializer& serializer) {
	}

	virtual Result LoadImpl(const U8* file, size_t file_size) {
		return Success();
	}

	static Asset* GetImpl(String name);

	template <typename T>
	static T* Get(String name) {
		Asset* asset = GetImpl(name);
		return dynamic_cast<T*>(asset);
	}
};

void    AssetInit(Asset* asset, const char* name);
void    AssetDeinit(Asset* asset);
Asset*  AssetGet(U32 id, Type* expected_type);
Asset*  AssetGetByName(const char* name, Type* expected_type);
void    AssetsSkipToId(U32 id);
void    AssetsLoad();