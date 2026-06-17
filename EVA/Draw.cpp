#include <EVA/Draw.hpp>
#include <EVA/GL.hpp>
#include <EVA/Platform.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vector>

static Mesh* DrawQuadMesh = nullptr;
static GLuint DrawBoxShader;
FT_Library FT;

#define FT_CHECK(expr) \
	do \
	{ \
		FT_Error err = (expr); \
		if (err != 0) Fatal("FT_Init_FreeType: %d", err); \
	} while (0)

void DrawInitialize()
{
	FT_CHECK(FT_Init_FreeType(&FT));

	DrawBoxShader = GLCompileShaderProgram("DrawBox");

	{ // DrawQuadMesh:
		MeshVertex quad_vertices[] = {
			MeshVertex { .position = float3(0, 0, 0) },
			MeshVertex { .position = float3(1, 0, 0) },
			MeshVertex { .position = float3(1, 1, 0) },
			MeshVertex { .position = float3(0, 1, 0) },
		};
		U32 quad_indices[] = { 0, 1, 2, 0, 2, 3 };
		DrawQuadMesh = MeshCreate("mesh_quad",
			EVA_ARRAYSIZE(quad_vertices), quad_vertices,
			EVA_ARRAYSIZE(quad_indices), quad_indices);
	}

}

void DrawContextInit(DrawContext& dc)
{
}

void DrawRender(DrawContext& dc)
{
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glUseProgram(DrawBoxShader);
	glBindVertexArray(DrawQuadMesh->vao);
	glUniform2f(0, WindowWidth, WindowHeight);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_ERROR_CHECK();

	int start = 0;
	while (start < dc.quads.size())
	{
		Texture* texture = dc.quads[start].texture;

		int end;
		for (end = start; end < dc.quads.size(); end++)
		{
			if (dc.quads[end].texture != texture)
			{
				break;
			}
		}

		std::vector<DrawQuad> quads(end - start);
		for (int i = start; i < end; i++)
		{
			DrawQuadRecord& record = dc.quads[i];
			quads[i - start] = DrawQuad{
				.mode          = record.mode,
				.position_rect = record.position_rect,
				.uv_rect       = record.uv_rect,
				.tint          = record.tint,
			};
		}

		GLuint quad_buffer;
		glGenBuffers(1, &quad_buffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, quad_buffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, quads.size() * sizeof(quads[0]), quads.data(), GL_STATIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, quad_buffer);
		GL_ERROR_CHECK();

		if (texture)
		{
			glBindTexture(GL_TEXTURE_2D, texture->handle);
			glActiveTexture(GL_TEXTURE0);
			glUniform1i(1, 0);
			GL_ERROR_CHECK();
		}

		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0, quads.size());
		GL_ERROR_CHECK();

		glDeleteBuffers(1, &quad_buffer);

		start = end;
	}
	dc.quads.clear();
}

static void Blit(
	U8* dst, int dst_width, int dst_x, int dst_y,
	U8* src, int src_width, int rows, int src_pitch)
{
	dst = dst + dst_x + dst_width * dst_y;
	
	for (int i = 0; i < rows; i++)
	{
		memcpy(dst, src, src_width);
		dst += dst_width;
		src += src_pitch;
	}
}

Font* FontLoad(const char* name, int size, int atlas_size)
{
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

	for (int charcode = ' '; charcode <= '~'; charcode++)
	{
		int glyph_index = FT_Get_Char_Index(font->face, charcode);
		FT_CHECK(FT_Load_Glyph(font->face, glyph_index, FT_LOAD_DEFAULT));
		FT_CHECK(FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL));

		FT_Bitmap& bitmap = font->face->glyph->bitmap;

		int width = bitmap.width;
		int height = bitmap.rows;

		if (atlas_x + width > atlas_size)
		{
			atlas_x = 0;
			atlas_y += row_size;
			row_size = 0;
		}

		int x = atlas_x;
		int y = atlas_y;

		atlas_x += width;
		if (row_size < height) row_size = height;

		if (y + row_size > atlas_size) Fatal("FontLoad: atlas too small\n");

		Blit(
			atlas_buffer, atlas_size, x, y,
			bitmap.buffer, width, height, bitmap.pitch);

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
	font->atlas = TextureCreate(texture_name, atlas_size, atlas_size, atlas_buffer, GL_R8);
	font->yoffset = font->glyphs['O'].height;

	return font;
}

void DrawRectangle(DrawContext& dc, int x, int y, int w, int h, float4 color)
{
	dc.quads.push_back(DrawQuadRecord{
		.mode = DrawQuadMode_SolidColor,
		.position_rect = float4(x, y, w, h),
		.uv_rect = {0,0,0,0},
		.tint = color,
	});
}

void DrawText(DrawContext& dc, Font* font, const char* text, int x, int y, float4 color)
{
	for (const char* ptr = text; *ptr; ptr++)
	{
		char c = *ptr;
		if (c < 0) continue;

		FontGlyph& glyph = font->glyphs[c];

		int xx = x + glyph.xoffs;
		int yy = y - glyph.yoffs + font->yoffset;

		dc.quads.push_back(DrawQuadRecord{
			.mode = DrawQuadMode_Text,
			.texture = font->atlas,
			.position_rect = float4(xx, yy, glyph.width, glyph.height),
			.uv_rect = float4(
				(float)glyph.x / (float)font->atlas->width,
				(float)glyph.y / (float)font->atlas->height,
				(float)glyph.width / (float)font->atlas->width,
				(float)glyph.height / (float)font->atlas->height),
			.tint = color,
		});

		x += glyph.advance;
	}

}

float MeasureText(Font* font, const char* text)
{
	float size = 0;
	for (const char* ptr = text; *ptr; ptr++)
	{
		char c = *ptr;
		if (c < 0) continue;

		FontGlyph& glyph = font->glyphs[c];
		size += glyph.advance;
	}
	return size;
}