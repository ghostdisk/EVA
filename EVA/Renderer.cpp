#include <EVA/Renderer.hpp>
#include <EVA/Game.hpp>
#include <EVA/Library.hpp>
#include <vector>
#include <tracy/Tracy.hpp>

GLuint LineShader;
GLuint MainShader;

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
};

static std::vector<LineVertex> pending_lines;
static std::vector<DrawMeshEntry> pending_meshes;

void RendererInitialize()
{
	LineShader = GLCompileShaderProgram("Lines");
	MainShader = GLCompileShaderProgram("Main");
}

void DrawLine(float3 a, float3 b, float4 color)
{
	pending_lines.push_back({ a, color });
	pending_lines.push_back({ b, color });
}

void RenderScene()
{
	ZoneScopedN("RenderScene");

	{ // render pending meshes:
		glUseProgram(MainShader);
		glUniformMatrix4fv(0, 1, false, (float*)&ActiveGame->camera.view_projection_matrix);


		for (const DrawMeshEntry& entry : pending_meshes)
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

		pending_meshes.clear();
	}

	{ // render pending lines:
		GLuint vao, vbo;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, pending_lines.size() * sizeof(pending_lines[0]), pending_lines.data(), GL_STATIC_DRAW);
		GL_ERROR_CHECK();

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(LineVertex), (void*)0);
		glVertexAttribPointer(1, 4, GL_FLOAT, false, sizeof(LineVertex), (void*)12);

		glUseProgram(LineShader);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUniformMatrix4fv(0, 1, false, (float*)&ActiveGame->camera.view_projection_matrix);
		glDrawArrays(GL_LINES, 0, pending_lines.size());
		GL_ERROR_CHECK();

		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);

		pending_lines.clear();
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

void DrawMesh(Mesh* mesh, Material* material, const float4x4& matrix)
{
	pending_meshes.push_back({ mesh, material, matrix });
}

void RendererBeginFrame()
{
	ZoneScoped;

	pending_meshes.clear();
	pending_lines.clear();
}