#pragma once
#include <EVA/Assets/Asset.hpp>

enum class ESERIALIZABLE TextureInterpolation {
	Point,
	Bilinear,
	Trilinear,
};

struct ESERIALIZABLE TextureProps {
	EPROPERTY()
	bool generate_mipmaps = true;

	EPROPERTY()
	TextureInterpolation interpolation = TextureInterpolation::Bilinear;
};

class ECLASS() Texture : public Asset {
public:
ECLASS_COMMON();
	TextureProps props = {};

	U32    handle = 0;
	size_t width  = 0;
	size_t height = 0;

	virtual Result LoadImpl(const U8* file, size_t file_size) override;

	void Upload(int width, int height, const U8* pixels, U32 gl_format);
};
