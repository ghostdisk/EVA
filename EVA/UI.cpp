#include <EVA/UI.hpp>
#include <EVA/GL.hpp>
#include <EVA/Platform.hpp>

static GLuint UIShader;
static Mesh* UIQuadMesh = nullptr;

void UIInitialize()
{
	UIShader = GLCompileShaderProgram("UIBox");

	{ // UIQuadMesh:
		MeshVertex quad_vertices[] = {
			MeshVertex { .position = float3(0, 0, 0) },
			MeshVertex { .position = float3(1, 0, 0) },
			MeshVertex { .position = float3(1, 1, 0) },
			MeshVertex { .position = float3(0, 1, 0) },
		};
		U32 quad_indices[] = { 0, 1, 2, 0, 2, 3 };
		UIQuadMesh = MeshCreate("mesh_quad",
			EVA_ARRAYSIZE(quad_vertices), quad_vertices,
			EVA_ARRAYSIZE(quad_indices), quad_indices);
	}
}

void UIContextInit(UIContext& ui)
{
}

UIBox* UIBeginBox(UIContext& ui, U32 id)
{
	return 0;
}

void UIEndBox(UIContext& ui)
{

}

void UIPushId(UIContext& ui, U32 id)
{

}

void UIPushId(UIContext& ui, const char* str)
{

}

void UIPopId(UIContext& ui)
{

}

void UIBeginFrame(UIContext& ui)
{
	ui.quads.push_back(UIQuad{
		.position_rect = { 0, 0, 200, 200 },
	});
	ui.quads.push_back(UIQuad{
		.position_rect = { 200, 200, 400, 200 },
	});
}

void UIEndFrame(UIContext& ui)
{

}

void UIRender(UIContext& ui)
{
	GLuint quad_buffer;
	{ // upload the quad_buffer:
		glGenBuffers(1, &quad_buffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, quad_buffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, ui.quads.size() * sizeof(ui.quads[0]), ui.quads.data(), GL_STATIC_DRAW);
	}

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, quad_buffer);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glUseProgram(UIShader);
	glBindVertexArray(UIQuadMesh->vao);
	GL_ERROR_CHECK();

	glUniform2f(0, WindowWidth, WindowHeight);
	GL_ERROR_CHECK();

	// glDrawElements(GL_TRIANGLES, UIQuadMesh->index_count, GL_UNSIGNED_INT, (void*)0);
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0, ui.quads.size());
	GL_ERROR_CHECK();

	ui.quads.clear();
	glDeleteBuffers(1, &quad_buffer);
}