#pragma once
#include <EVA/Common.hpp>

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

void GLInit();
GLuint GLCompileShaderProgram(const char* name);

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