#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

void GLInitialize();
GLuint GLCompileShaderProgram(const char* name);

struct MeshVertex
{
	float3 position;
	float3 normal;
	float2 texcoord;
};

struct Mesh
{
	char   name[32]    = {};
	GLuint vao         = 0;
	GLuint vbo         = 0;
	GLuint ibo         = 0;
	U32    index_count = 0;
};

#define GL_ERROR_CHECK() \
	do \
	{ \
		GLenum err = glGetError(); \
		if (err != GL_NO_ERROR) \
		{ \
 			GL_ERROR_CHECK_Impl(__FILE__, __LINE__, err); \
		} \
	} while (0)


void GL_ERROR_CHECK_Impl(const char* file, int line, GLenum error);

Mesh* CreateMesh(
	const char* name,
	size_t num_vertices, const MeshVertex* vertices,
	size_t num_indices, const U32* indices);