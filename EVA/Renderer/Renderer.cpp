#include <EVA/Assets/Asset.hpp>
#include <EVA/Game.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/Font.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Library.hpp>
#include <EVA/Console.hpp>
#include <vector>
#include <tracy/Tracy.hpp>

// ------------------------------------------------------------

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

struct MainConstantBuffer
{
	float4x4 view;
	float4x4 view_projection;
};

// ------------------------------------------------------------

Shader* shd_lines;
Shader* shd_main;
Shader* shd_quad;
Shader* shd_brush;

static Mesh* mesh_quad = nullptr;

GLuint g_main_constant_buffer;

static LayerData layers[Layer_ENUM_SIZE] = {};
static LayerData* current_layer = &layers[Layer_Main];

// ------------------------------------------------------------

ConVar cvar_gl_wire = {
	.name = "gl_wire",
	.help = "toggle opengl wireframe mode",
	.fvalue = 0,
};

void RendererInitialize()
{
	shd_lines   = GLCompileShader("Lines");
	shd_main    = GLCompileShader("Main");
	shd_quad    = GLCompileShader("DrawBox");

	const char* brush_defines[] = { "S_BRUSH" };
	shd_brush = GLCompileShader("Main", EVA_ARRAYSIZE(brush_defines), brush_defines);

	{ // mesh_quad:
		MeshVertex quad_vertices[] = {
			MeshVertex { .position = float3(0, 0, 0) },
			MeshVertex { .position = float3(1, 0, 0) },
			MeshVertex { .position = float3(1, 1, 0) },
			MeshVertex { .position = float3(0, 1, 0) },
		};
		U32 quad_indices[] = { 0, 1, 2, 0, 2, 3 };
		mesh_quad = MeshCreate("mesh_quad",
			EVA_ARRAYSIZE(quad_vertices), quad_vertices,
			EVA_ARRAYSIZE(quad_indices), quad_indices);
	}

	ConRegisterVar(&cvar_gl_wire);

	glGenBuffers(1, &g_main_constant_buffer);
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

void DrawSetLineThickness()
{
}

void RenderFrame()
{
	ZoneScopedN("RenderScene");

	MainConstantBuffer cb = {};
	if (g_current_camera)
	{
		cb.view = g_current_camera->view_matrix;
		cb.view_projection = g_current_camera->view_projection_matrix;
	}
	glBindBuffer(GL_UNIFORM_BUFFER, g_main_constant_buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(cb), &cb, GL_STATIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_main_constant_buffer);

	glViewport(0, 0, g_window_size.x, g_window_size.y);
	glDepthRange(0.0, 1.0);

	if (glClipControl) glClipControl(GL_LOWER_LEFT,  GL_ZERO_TO_ONE);

	for (Layer layer = (Layer)0; layer < Layer_ENUM_SIZE; layer = (Layer)(layer + 1))
	{
		DrawSetLayer(layer);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glDisable(GL_BLEND);

		if (layer == Layer_Sky)
		{
			float4 clear_color = COLOR_BLACK;
			glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		bool layer_has_depth = layer == Layer_Main;

		if (layer_has_depth)
		{
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		{ // render pending meshes:
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glPolygonMode(GL_FRONT_AND_BACK, cvar_gl_wire.fvalue ?  GL_LINE : GL_FILL);

			for (const DrawMeshEntry& entry : current_layer->meshes)
			{
				Material* material      = entry.material;
				Shader*   shader        = shd_main;
				Texture*  color_texture = Library::tex_proto;

				if (!material)
				{
					material = entry.mesh->default_maerial;
				}
				if (material)
				{
					shader = material->shader;
					if (material->color_texture)
					{
						color_texture = material->color_texture;
					}
				}

				glUseProgram(shader->handle);
				// glUniformMatrix4fv(0, 1, false, (float*)&g_current_camera->view_projection_matrix);
				// glUniformMatrix4fv(4, 1, false, (float*)&g_current_camera->view_matrix);
				GL_ERROR_CHECK();

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, color_texture->handle);
				glUniformMatrix4fv(2, 1, false, (float*)&entry.matrix);
				glUniform4fv(3, 1, &entry.color.x);
				glBindVertexArray(entry.mesh->vao);
				GL_ERROR_CHECK();

				if (entry.mesh->index_count)
				{
					glDrawElements(GL_TRIANGLES, entry.mesh->index_count, GL_UNSIGNED_INT, (void*)0);
				}
				else
				{
					glDrawArrays(GL_TRIANGLES, 0, entry.mesh->vertex_count);
				}
				GL_ERROR_CHECK();
			}

			current_layer->meshes.clear();
		}

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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

			glUseProgram(shd_lines->handle);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
			glUseProgram(shd_quad->handle);
			glBindVertexArray(mesh_quad->vao);
			glUniform2f(0, g_window_size.x, g_window_size.y);
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
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, texture->handle);
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