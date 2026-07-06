#pragma once
#include <EVA/Object.hpp>

class ECLASS() Asset : public Object {
public:
	ECLASS_COMMON();

	U32       id       = 0;
	char      name[64] = {};
};

void    AssetInit(Asset* asset, const char* name);
void    AssetDeinit(Asset* asset);
Asset*  AssetGet(U32 id, Type* expected_type);
Asset*  AssetGetByName(const char* name, Type* expected_type);
void    AssetsSkipToId(U32 id);
void    AssetsInitialize();