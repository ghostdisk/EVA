#include <EVA/Draw.hpp>
#include <EVA/GL.hpp>
#include <EVA/Platform.hpp>

static Mesh* DrawQuadMesh = nullptr;
static GLuint DrawBoxShader;

void DrawInitialize()
{
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
	GLuint quad_buffer;
	{ // upload the quad_buffer:
		glGenBuffers(1, &quad_buffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, quad_buffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, dc.quads.size() * sizeof(dc.quads[0]), dc.quads.data(), GL_STATIC_DRAW);
	}

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, quad_buffer);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glUseProgram(DrawBoxShader);
	glBindVertexArray(DrawQuadMesh->vao);
	GL_ERROR_CHECK();

	glUniform2f(0, WindowWidth, WindowHeight);
	GL_ERROR_CHECK();

	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0, dc.quads.size());
	GL_ERROR_CHECK();

	dc.quads.clear();
	glDeleteBuffers(1, &quad_buffer);

}
