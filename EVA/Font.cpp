#include <EVA/GFX/Renderer.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/Assets/Font.hpp>
#include <EVA/Assets/Texture.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

FT_Library FT;

#define FT_CHECK(expr) \
	do \
	{ \
		FT_Error err = (expr); \
		if (err != 0) Fatal("FT_Init_FreeType: %d", err); \
	} while (0)

void FontInitialize() {
	ZoneScopedN("FontInitialize");

	FT_CHECK(FT_Init_FreeType(&FT));
}

static void Blit( U8* dst, int dst_width, int dst_x, int dst_y, U8* src, int src_width, int rows, int src_pitch) {
	dst = dst + dst_x + dst_width * dst_y;
	
	for (int i = 0; i < rows; i++) {
		memcpy(dst, src, src_width);
		dst += dst_width;
		src += src_pitch;
	}
}

Font* FontLoad(const char* name, int size, int atlas_size) {
	printf("[asset] FontLoad uses old style\n");
	Font* font = new Font();

	char path[256];
	snprintf(path, sizeof(path), "%s/Assets/%s", EVA_BASE_DIR, name);

	FT_CHECK(FT_New_Face(FT, path, 0, &font->face));
	FT_CHECK(FT_Set_Pixel_Sizes(font->face, 0, size));

	U8* atlas_buffer = (U8*)malloc(atlas_size * atlas_size);
	DEFER(free(atlas_buffer));
	memset(atlas_buffer, 0, atlas_size * atlas_size);

	int atlas_x = 0;
	int atlas_y = 0;
	int row_size = 0;

	for (int charcode = ' '; charcode <= '~'; charcode++) {
		int glyph_index = FT_Get_Char_Index(font->face, charcode);
		FT_CHECK(FT_Load_Glyph(font->face, glyph_index, FT_LOAD_DEFAULT));
		FT_CHECK(FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL));

		FT_Bitmap& bitmap = font->face->glyph->bitmap;

		int width = bitmap.width;
		int height = bitmap.rows;

		if (atlas_x + width > atlas_size) {
			atlas_x = 0;
			atlas_y += row_size;
			row_size = 0;
		}

		int x = atlas_x;
		int y = atlas_y;

		atlas_x += width;
		if (row_size < height) row_size = height;

		if (y + row_size > atlas_size) Fatal("FontLoad: atlas too small\n");

		Blit(atlas_buffer, atlas_size, x, y, bitmap.buffer, width, height, bitmap.pitch);

		font->glyphs[charcode].x = x;
		font->glyphs[charcode].y = y;
		font->glyphs[charcode].width = width;
		font->glyphs[charcode].height = height;
		font->glyphs[charcode].advance = font->face->glyph->advance.x >> 6;
		font->glyphs[charcode].xoffs = font->face->glyph->bitmap_left;
		font->glyphs[charcode].yoffs = font->face->glyph->bitmap_top;
	}

	char texture_name[256];
	snprintf(texture_name, 256, "%s_atlas", name);
	font->atlas = new Texture();
	font->atlas->m_generateMipmaps = false;
	font->atlas->Upload(atlas_size, atlas_size, atlas_buffer, GPUFormat::R8_UNORM);
	font->pixel_size = font->glyphs['O'].height;
	font->line_height = font->pixel_size * 1.5;

	return font;
}

