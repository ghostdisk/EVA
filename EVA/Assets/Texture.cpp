#include <EVA/Core/Serialization.hpp>
#include <EVA/Assets/Texture.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <Vendor/stb_image.h>

void Texture::LoadMetaImpl(Deserializer& d) {
	d.BeginObject();
	d.Key("version");
	U32 version = d.DeserializeU8();
	if (version != 2) {
		d.res = Err("Unexpected version");
		return;
	}
	d.Key("generate_mipmaps");
	props.generate_mipmaps = d.DeserializeBool();
	d.Key("interpolation");
	props.interpolation = (TextureInterpolation)d.DeserializeU8();
	d.Key("wrap_mode");
	props.wrap_mode = (TextureWrapMode)d.DeserializeU8();
	d.EndObject();
}

void Texture::SaveMetaImpl(Serializer& s) {
	s.BeginObject();
	s.Key("version");
	s.SerializeU8(2);
	s.Key("generate_mipmaps");
	s.SerializeBool(props.generate_mipmaps);
	s.Key("interpolation");
	s.SerializeU8((U8)props.interpolation);
	s.Key("wrap_mode");
	s.SerializeU8((U8)props.wrap_mode);
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
	if (props.generate_mipmaps) {
		U32 size = width > height ? width : height;
		while (size > 1) {
			size >>= 1;
			mipCount++;
		}
	}

	GFX::GraphicsDevice* device = GFX::GraphicsDevice::Get();
	image = device->CreateImage(GFX::ImageDesc{
		.width = (U32)width,
		.height = (U32)height,
		.mipCount = mipCount,
		.format = format,
		.usage = GFX::ImageUsage_TransferSource | GFX::ImageUsage_TransferDest | GFX::ImageUsage_Sampled,
		.initialState = GFX::ImageState::Undefined,
		.bindless = true,
	});
	device->UploadImage(image, (U64)width * height * (format == GFX::Format::R8_UNORM ? 1 : 4), pixels);

	GFX::Filter filter = props.interpolation == TextureInterpolation::Point ? GFX::Filter::Nearest : GFX::Filter::Linear;
	GFX::AddressMode addressMode = GFX::AddressMode::Repeat;
	switch (props.wrap_mode) {
		case TextureWrapMode::Clamp:          addressMode = GFX::AddressMode::ClampToEdge; break;
		case TextureWrapMode::Repeat:         addressMode = GFX::AddressMode::Repeat; break;
		case TextureWrapMode::MirroredRepeat: addressMode = GFX::AddressMode::MirroredRepeat; break;
	}

	sampler = device->CreateSampler(GFX::SamplerDesc{
		.minFilter = filter,
		.magFilter = filter,
		.mipmapMode = props.interpolation == TextureInterpolation::Trilinear ? GFX::MipmapMode::Linear : GFX::MipmapMode::Nearest,
		.addressU = addressMode,
		.addressV = addressMode,
		.addressW = addressMode,
		.maxLod = (float)mipCount,
		.bindless = true,
	});
}
