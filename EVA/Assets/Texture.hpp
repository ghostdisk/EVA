#pragma once
#include <EVA/Assets/Asset.hpp>

class ECLASS() Texture : public Asset {
public:
ECLASS_COMMON();
	U32    handle = 0;
	size_t width  = 0;
	size_t height = 0;

	virtual Result LoadImpl(const U8* file, size_t file_size) override;

	void Upload(int width, int height, const U8* pixels, U32 gl_format, bool mips);
};