#pragma once
#include <EVA/Assets/Asset.hpp>
#include <EVA/GFX/GraphicsDevice.hpp>

class Texture : public Asset {
public:
ECLASS_COMMON();
	Texture();

	bool          m_generateMipmaps = true;
	Sampler* m_sampler = nullptr;

	Image*   image   = nullptr;
	size_t        width   = 0;
	size_t        height  = 0;

	virtual void LoadMetaImpl(Deserializer& deserializer) override;
	virtual void SaveMetaImpl(Serializer& serializer) override;
	virtual Result LoadImpl(FILE* f) override;

	void Upload(int width, int height, const U8* pixels, Format format);
};
