#include <EVA/Assets/Texture.hpp>
#include <EVA/Renderer/GL.hpp>
#include <Vendor/stb_image.h>

Result Texture::LoadImpl(const U8* file, size_t file_size) {
	int width, height, num_channels_in_file;
	U8* pixels = stbi_load_from_memory(file, file_size, &width, &height, &num_channels_in_file, 4);
	if (!pixels) return Err("failed to parse image");
	DEFER(free(pixels));

	char name_without_ext[64];
	int len = snprintf(name_without_ext, 64, "%s", name);
	ReplaceFileExtension(name_without_ext, 64, "");

	Upload(width, height, pixels, GL_RGBA8, true);
	return Success();
}