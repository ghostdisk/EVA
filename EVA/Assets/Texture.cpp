#include <EVA/Core/Serialization.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Renderer/GL.hpp>
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

Result Texture::LoadImpl(const U8* file, size_t file_size) {
	int width, height, num_channels_in_file;
	U8* pixels = stbi_load_from_memory(file, file_size, &width, &height, &num_channels_in_file, 4);
	if (!pixels) return Err("failed to parse image");
	DEFER(free(pixels));

	char name_without_ext[64];
	int len = snprintf(name_without_ext, 64, "%s", name);
	ReplaceFileExtension(name_without_ext, 64, "");

	Upload(width, height, pixels, GL_RGBA8);
	return Success();
}
