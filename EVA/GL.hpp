#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <EVA/Asset.hpp>

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

struct Material;
struct Collider;

struct Mesh : Asset
{
	GLuint        vao             = 0;
	GLuint        vbo             = 0;
	GLuint        ibo             = 0;
	U32           index_count     = 0;
	U32           vertex_count    = 0;
	Material*     default_maerial = nullptr;
	Collider*     collider        = nullptr;
};

struct Texture : Asset
{
	GLuint handle = 0;
	size_t width  = 0;
	size_t height = 0;
};

struct Material : Asset
{
	Texture* color_texture = nullptr;
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

Mesh* MeshCreate(
	const char* name,
	size_t num_vertices, const MeshVertex* vertices,
	size_t num_indices, const U32* indices);

Texture* TextureCreate(const char* name, int width, int height, const U8* pixels, GLenum format);
Texture* TextureLoad(const char* name);
Material* MaterialCreate(const char* name, Texture* texture);
void MeshDestroy(Mesh* mesh);