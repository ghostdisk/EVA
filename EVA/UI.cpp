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

}

void UIEndFrame(UIContext& ui)
{

}

void UIRender(UIContext& ui)
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glUseProgram(UIShader);
	glBindVertexArray(UIQuadMesh->vao);
	GL_ERROR_CHECK();

	glUniform2f(0, WindowWidth, WindowHeight);
	GL_ERROR_CHECK();

	glDrawElements(GL_TRIANGLES, UIQuadMesh->index_count, GL_UNSIGNED_INT, (void*)0);
	GL_ERROR_CHECK();

}