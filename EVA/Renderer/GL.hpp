#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <glad/glad.h>

class Mesh;
class Material;
class Texture;
class Shader;

void GLPreInitialize();
void GLInitialize();
Shader* GLCompileShader(const char* name, int num_defines = 0, const char** defines = nullptr);

struct MeshVertex {
	float3 position;
	float3 normal;
	float2 texcoord;
};

#define GL_ERROR_CHECK() \
	do { \
		GLenum err = glGetError(); \
		if (err != GL_NO_ERROR) { \
 			GL_ERROR_CHECK_Impl(__FILE__, __LINE__, err); \
		} \
	} while (0)


void GL_ERROR_CHECK_Impl(const char* file, int line, GLenum error);

Mesh* MeshCreate( const char* name, size_t num_vertices, const MeshVertex* vertices, size_t num_indices, const U32* indices);
void  MeshDestroy(Mesh* mesh);
