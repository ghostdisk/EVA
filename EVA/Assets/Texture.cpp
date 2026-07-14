#include <EVA/Core/Serialization.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Renderer/Renderer.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <Vendor/stb_image.h>

Texture::Texture() {
	m_sampler = g_standardSamplers[8];
}

void Texture::LoadMetaImpl(Deserializer& d) {
	d.BeginObject();
	d.Key("version");
	U32 version = d.DeserializeU8();
	if (version < 2 || version > 3) {
		d.res = Err("Unexpected version");
		return;
	}

	d.Key("generate_mipmaps");
	m_generateMipmaps = d.DeserializeBool();

	U32 samplerIdx =0 ;
	if (version == 2) {
		d.Key("interpolation");
		d.DeserializeU8();
		d.Key("wrap_mode");
		d.DeserializeU8();
		samplerIdx = (U32)StandardSampler::TrilinearWrap;
	} else {
		d.Key("sampler");
		samplerIdx = d.DeserializeU32();
	}

	m_sampler = g_standardSamplers[samplerIdx];
	d.EndObject();
}

void Texture::SaveMetaImpl(Serializer& s) {
	s.BeginObject();
	s.Key("version");
	s.SerializeU8(3);
	s.Key("generate_mipmaps");
	s.SerializeBool(m_generateMipmaps);
	s.Key("sampler");
	s.SerializeU32(m_sampler ? m_sampler->m_bindlessIndex : (U32)StandardSampler::TrilinearWrap);
	s.EndObject();
}

Result Texture::LoadImpl(FILE* f) {
	int width, height, num_channels_in_file;
	U8* pixels = stbi_load_from_file(f, &width, &height, &num_channels_in_file, 4);
	if (!pixels) return Err("failed to parse image");
	DEFER(free(pixels));

	Upload(width, height, pixels, GFX::Format::RGBA8_SRGB);
	return Success();
}

void Texture::Upload(int width, int height, const U8* pixels, GFX::Format format) {
	this->width = width;
	this->height = height;

	U32 mipCount = 1;
	// if (props.generate_mipmaps) {
	// 	U32 size = width > height ? width : height;
	// 	while (size > 1) {
	// 		size >>= 1;
	// 		mipCount++;
	// 	}
	// }

	GFX::GraphicsDevice* device = GFX::GraphicsDevice::Get();
	image = device->CreateImage(GFX::ImageDesc{
		.width        = (U32)width,
		.height       = (U32)height,
		.mipCount     = mipCount,
		.format       = format,
		.usage        = GFX::ImageUsage_TransferSource | GFX::ImageUsage_TransferDest | GFX::ImageUsage_Sampled,
		.initialState = GFX::ImageState::Undefined,
		.bindless     = true,
	});
	device->UploadImage(image, (U64)width * height * (format == GFX::Format::R8_UNORM ? 1 : 4), pixels);

}
