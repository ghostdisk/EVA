#include <EVA/Asset.hpp>
#include <EVA/Game.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Library.hpp>
#include <vector>
#include <tracy/Tracy.hpp>

static GLuint LineShader;
static GLuint MainShader;
static GLuint DrawBoxShader;
static Mesh* DrawQuadMesh = nullptr;

struct LineVertex
{
	float3 position;
	float4 color;
};

struct DrawMeshEntry
{
	Mesh*     mesh;
	Material* material;
	float4x4  matrix;
	float4    color;
};

enum DrawCommandType
{
	DrawCommandType_None,
	DrawCommandType_Lines,
	DrawCommandType_Quads,
	DrawCommandType_Meshes,
};

struct LayerData
{
	std::vector<LineVertex> lines;
	std::vector<DrawMeshEntry> meshes;
	std::vector<DrawQuadRecord> quads;
};

LayerData layers[Layer_ENUM_SIZE] = {};
LayerData* current_layer = &layers[Layer_Main];

void RendererInitialize()
{
	LineShader    = GLCompileShaderProgram("Lines");
	MainShader    = GLCompileShaderProgram("Main");
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

void DrawLine(float3 a, float3 b, float4 color)
{
	current_layer->lines.push_back({ a, color });
	current_layer->lines.push_back({ b, color });
}

void DrawSetLayer(Layer l)
{
	current_layer = &layers[l];
}

void RenderFrame()
{
	ZoneScopedN("RenderScene");

	glViewport(0, 0, WindowWidth, WindowHeight);


	for (Layer layer = (Layer)0; layer < Layer_ENUM_SIZE; layer = (Layer)(layer + 1))
	{
		DrawSetLayer(layer);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glDisable(GL_BLEND);

		if (layer == Layer_Sky)
		{
			float4 clear_color = COLOR_SKY;
			glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		bool layer_has_depth = layer == Layer_Main;

		if (layer_has_depth)
		{
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		{ // render pending meshes:
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glUseProgram(MainShader);
			glUniformMatrix4fv(0, 1, false, (float*)&ActiveGame->camera.view_projection_matrix);

			for (const DrawMeshEntry& entry : current_layer->meshes)
			{
				Material* material = entry.material;
				if (!material) material = entry.mesh->default_maerial;

				Texture* color_texture = Library::tex_proto;
				if (material && material->color_texture)
				{
					color_texture = material->color_texture;
				}

				glBindTexture(GL_TEXTURE_2D, color_texture->handle);
				glActiveTexture(GL_TEXTURE0);
				glUniformMatrix4fv(2, 1, false, (float*)&entry.matrix);
				glUniform4fv(3, 1, &entry.color.x);
				glBindVertexArray(entry.mesh->vao);

				if (entry.mesh->index_count)
				{
					glDrawElements(GL_TRIANGLES, entry.mesh->index_count, GL_UNSIGNED_INT, (void*)0);
				}
				else
				{
					glDrawArrays(GL_TRIANGLES, 0, entry.mesh->vertex_count);
				}
			}

			current_layer->meshes.clear();
		}

		// render pending lines:
		if (current_layer->lines.size())
		{ 
			GLuint vao, vbo;
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, current_layer->lines.size() * sizeof(current_layer->lines[0]), current_layer->lines.data(), GL_STATIC_DRAW);
			GL_ERROR_CHECK();

			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(LineVertex), (void*)0);
			glVertexAttribPointer(1, 4, GL_FLOAT, false, sizeof(LineVertex), (void*)12);

			glUseProgram(LineShader);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glUniformMatrix4fv(0, 1, false, (float*)&ActiveGame->camera.view_projection_matrix);
			glDrawArrays(GL_LINES, 0, current_layer->lines.size());
			GL_ERROR_CHECK();

			glDeleteVertexArrays(1, &vao);
			glDeleteBuffers(1, &vbo);

			current_layer->lines.clear();
		}

		// render quads:
		{
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			glUseProgram(DrawBoxShader);
			glBindVertexArray(DrawQuadMesh->vao);
			glUniform2f(0, WindowWidth, WindowHeight);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GL_ERROR_CHECK();

			int start = 0;
			while (start < current_layer->quads.size())
			{
				Texture* texture = current_layer->quads[start].texture;

				int end;
				for (end = start; end < current_layer->quads.size(); end++)
				{
					if (current_layer->quads[end].texture != texture)
					{
						break;
					}
				}

				std::vector<DrawQuad> quads(end - start);
				for (int i = start; i < end; i++)
				{
					DrawQuadRecord& record = current_layer->quads[i];
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
			current_layer->quads.clear();
		}

	}

}

void DrawGrid(int size)
{
	float4 color = {1,1,1,0.2};
	float s = size;
	for (int i = -size; i <= size; i++)
	{
		DrawLine({(float)i, -s, 0}, {(float)i, (float)size, 0}, color);
		DrawLine({-s, (float)i, 0}, {(float)size, (float)i, 0}, color);
	}
}

void DrawMesh(Mesh* mesh, Material* material, const float4x4& matrix, float4 color)
{
	current_layer->meshes.push_back({ mesh, material, matrix, color });
}

void DrawAABB(float3 center, float3 size, float4 color)
{
	float3 hsize = size / 2.0f;
	DrawLine(center + float3(-hsize.x, -hsize.y, -hsize.z), center + float3( hsize.x, -hsize.y, -hsize.z), color);
	DrawLine(center + float3(-hsize.x,  hsize.y, -hsize.z), center + float3( hsize.x,  hsize.y, -hsize.z), color);
	DrawLine(center + float3(-hsize.x, -hsize.y, -hsize.z), center + float3(-hsize.x,  hsize.y, -hsize.z), color);
	DrawLine(center + float3( hsize.x, -hsize.y, -hsize.z), center + float3( hsize.x,  hsize.y, -hsize.z), color);

	DrawLine(center + float3(-hsize.x, -hsize.y,  hsize.z), center + float3( hsize.x, -hsize.y,  hsize.z), color);
	DrawLine(center + float3(-hsize.x,  hsize.y,  hsize.z), center + float3( hsize.x,  hsize.y,  hsize.z), color);
	DrawLine(center + float3(-hsize.x, -hsize.y,  hsize.z), center + float3(-hsize.x,  hsize.y,  hsize.z), color);
	DrawLine(center + float3( hsize.x, -hsize.y,  hsize.z), center + float3( hsize.x,  hsize.y,  hsize.z), color);

	DrawLine(center + float3(-hsize.x, -hsize.y, -hsize.z), center + float3(-hsize.x, -hsize.y,  hsize.z), color);
	DrawLine(center + float3(-hsize.x,  hsize.y, -hsize.z), center + float3(-hsize.x,  hsize.y,  hsize.z), color);
	DrawLine(center + float3( hsize.x, -hsize.y, -hsize.z), center + float3( hsize.x, -hsize.y,  hsize.z), color);
	DrawLine(center + float3( hsize.x,  hsize.y, -hsize.z), center + float3( hsize.x,  hsize.y,  hsize.z), color);
}

void DrawPoint(float3 point, float4 color)
{
	DrawLine(point - float3(0.1, 0, 0), point + float3(0.1, 0, 0), color);
	DrawLine(point - float3(0, 0.1, 0), point + float3(0, 0.1, 0), color);
	DrawLine(point - float3(0,0,  0.1), point + float3(0,0,  0.1), color);
}

void DrawRectangle(float4 color, int x, int y, int w, int h)
{
	current_layer->quads.push_back(DrawQuadRecord{
		.mode = DrawQuadMode_SolidColor,
		.position_rect = float4(x, y, w, h),
		.uv_rect = {0,0,0,0},
		.tint = color,
	});
}



void DrawSprite(Sprite* sprite, int x, int y, float4 tint)
{
	current_layer->quads.push_back(DrawQuadRecord{
		.mode = DrawQuadMode_Sprite,
		.texture = sprite->texture,
		.position_rect = float4(x, y, sprite->w, sprite->h),
		.uv_rect = float4(
			(float)sprite->x / (float)sprite->texture->width,
			(float)sprite->y / (float)sprite->texture->height,
			(float)sprite->w / (float)sprite->texture->width,
			(float)sprite->h / (float)sprite->texture->height),
		.tint = tint,
	});
}

void DrawText(Font* font, const char* text, int len, int x, int y, float4 color)
{
	if (len < 0) len = strlen(text);

	float startx = x;
	y += font->pixel_size;;
	for (int i = 0; i < len; i++)
	{
		char c = text[i];
		if (c < 0) continue;

		FontGlyph& glyph = font->glyphs[c];

		if (c == '\n')
		{
			x = startx;
			y += font->line_height;
		}

		int xx = x + glyph.xoffs;
		int yy = y - glyph.yoffs;

		current_layer->quads.push_back(DrawQuadRecord{
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

float2 MeasureText(Font* font, const char* text, int len)
{
	if (len < 0) len = strlen(text);

	float row = 0;
	float2 size = {};
	size.y = font->pixel_size;

	for (int i = 0; i < len; i++)
	{
		char c = text[i];
		if (c < 0) continue;

		FontGlyph& glyph = font->glyphs[c];

		if (c == '\n')
		{
			row = 0;
			size.y += font->line_height;
		}

		row += glyph.advance;
		if (row > size.x) size.x = row;
	}
	return size;
}