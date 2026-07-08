#pragma once
#include <EVA/Core/Common.hpp>
#include <EVA/Math.hpp>
#include <glad/glad.h>

class Mesh;
class Material;
class Texture;
class Shader;
struct MeshVertex;

void GLPreInitialize();
void GLInitialize();
Shader* GLCompileShader(const char* name, int num_defines = 0, const char** defines = nullptr);

#define GL_ERROR_CHECK() \
	do { \
		GLenum err = glGetError(); \
		if (err != GL_NO_ERROR) { \
 			GL_ERROR_CHECK_Impl(__FILE__, __LINE__, err); \
		} \
	} while (0)


void GL_ERROR_CHECK_Impl(const char* file, int line, GLenum error);
