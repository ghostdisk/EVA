#include <EVA/Core/Serialization.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/GFX/Renderer.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <Vendor/stb_image.h>

Texture::Texture() {
	if (GPUDevice* device = GPUDevice::Get())
		m_sampler = device->GetSampler((U32)StandardSampler::TrilinearWrap);
}

bool Texture::HasMeta() {
	return true;
}

// @TODO @CLEANUP: Use the new auitoserializer for TextureMeta
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

	U32 samplerIdx = 0;
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

	m_sampler = GPUDevice::Get()->GetSampler(samplerIdx);
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

Result Texture::LoadImpl(Deserializer& d) {
	int width, height, num_channels_in_file;
	U8* pixels = stbi_load(m_fsPath.c_str(), &width, &height, &num_channels_in_file, 4);
	if (!pixels) return Err("failed to parse image");
	DEFER(free(pixels));

	// @TODO @THREADING - As it is right now, Upload should only be ever called from MT. We're not on MT.
	Upload(width, height, pixels, GPUFormat::RGBA8_SRGB);
	return Success();
}

void Texture::Upload(int width, int height, const U8* pixels, GPUFormat format) {
	this->width = width;
	this->height = height;

	// @TODO - Mipmaps are not implemented
	U32 mipCount = 1;
	// if (props.generate_mipmaps) {
	// 	U32 size = width > height ? width : height;
	// 	while (size > 1) {
	// 		size >>= 1;
	// 		mipCount++;
	// 	}
	// }

	GPUDevice* device = GPUDevice::Get();
	image = device->CreateImage({
		.name         = "texture image",
		.width        = (U32)width,
		.height       = (U32)height,
		.mipCount     = mipCount,
		.format       = format,
		.usage        = GPUImageUsage_TransferSource | GPUImageUsage_TransferDest | GPUImageUsage_Sampled,
		.initialState = GPUImageState::Undefined,
		.bindless     = true,
	});
	device->UploadImage(image, (U64)width * height * (format == GPUFormat::R8_UNORM ? 1 : 4), pixels);

}
