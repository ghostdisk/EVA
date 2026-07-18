#pragma once
#include <EVA/Assets/Asset.hpp>
#include <EVA/GFX/GPUDevice.hpp>

class Texture : public Asset {
public:
ECLASS_COMMON(Texture);
	Texture();

	bool          m_generateMipmaps = true;
	GPUSampler* m_sampler = nullptr;

	GPUImage*   image   = nullptr;
	size_t        width   = 0;
	size_t        height  = 0;

	virtual AssetLoadType GetLoadType() override {
		return AssetLoadType::File;
	}

	virtual bool HasMeta() override;
	virtual void LoadMetaImpl(Deserializer& deserializer) override;
	virtual void SaveMetaImpl(Serializer& serializer) override;
	virtual Result LoadImpl(Deserializer& d) override;

	void Upload(int width, int height, const U8* pixels, GPUFormat format);
};
