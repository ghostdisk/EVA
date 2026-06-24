#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <glad/glad.h>

struct Mesh;
struct Material;
struct Texture;

void GLInitialize();
GLuint GLCompileShaderProgram(const char* name);

struct MeshVertex
{
	float3 position;
	float3 normal;
	float2 texcoord;
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

Mesh* MeshCreate( const char* name, size_t num_vertices, const MeshVertex* vertices, size_t num_indices, const U32* indices);
void  MeshDestroy(Mesh* mesh);

Texture* TextureCreate(const char* name, int width, int height, const U8* pixels, GLenum format);
Texture* TextureLoad(const char* name);

Material* MaterialCreate(const char* name, Texture* texture);