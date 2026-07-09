#pragma once
#include <EVA/Assets/Asset.hpp>

enum class ESERIALIZABLE TextureInterpolation : U8 {
	Point,
	Bilinear,
	Trilinear,
};

enum class ESERIALIZABLE TextureWrapMode : U8 {
	Clamp,
	Repeat,
	MirroredRepeat,
};

struct ESERIALIZABLE TextureProps {
	EPROPERTY()
	bool generate_mipmaps = true;

	EPROPERTY()
	TextureInterpolation interpolation = TextureInterpolation::Bilinear;

	EPROPERTY()
	TextureWrapMode wrap_mode = TextureWrapMode::Repeat;
};

class ECLASS() Texture : public Asset {
public:
ECLASS_COMMON();
	TextureProps props = {};

	U32    handle = 0;
	size_t width  = 0;
	size_t height = 0;

	virtual void LoadMetaImpl(Deserializer& deserializer) override;
	virtual void SaveMetaImpl(Serializer& serializer) override;
	virtual Result LoadImpl(FILE* f) override;

	void Upload(int width, int height, const U8* pixels, U32 gl_format);
};
