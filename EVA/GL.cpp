#include <EVA/GL.hpp>
#include <stdio.h>

SDL_GLContext GL = nullptr;

#define STB_IMAGE_IMPLEMENTATION
#include <Vendor/stb_image.h>

void GLInitialize()
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	GL = SDL_GL_CreateContext(GameWindow);
	if (!GL)
	{
		Fatal("SDL_GL_CreateContext: %s", SDL_GetError());
	}

	if (!SDL_GL_MakeCurrent(GameWindow, GL))
	{
		Fatal("SDL_GL_MakeCurrent: %s", SDL_GetError());
	}

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
	{
		Fatal("gladLoadGLLoader failed");
	}
}

static const char* GLLoadShaderSource(const char* name, GLenum stage)
{
	char path[256];
	snprintf(path, sizeof(path), "%s/EVA/Shaders/%s.%s.glsl", EVA_BASE_DIR, name, (stage == GL_VERTEX_SHADER ? "vs" : "fs"));
	
	void* data;
	if (!ReadEntireFile(path, &data, nullptr))
	{
		Fatal("GLLoadShaderSource: Failed to read %s\n", path);
	}

	return (const char*)data;
}

static GLuint GLCompileShader(const char* name, GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	int success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char error_buffer[2048];
		GLsizei error_length;
		glGetShaderInfoLog(shader, sizeof(error_buffer), &error_length, error_buffer);
		Fatal("Failed to compile %s:\n\n%s", name, error_buffer);
	}

	GL_ERROR_CHECK();
	return shader;
}

GLuint GLCompileShaderProgram(const char* name)
{
	GLuint program = glCreateProgram();

	const char* vs_src = GLLoadShaderSource(name, GL_VERTEX_SHADER);
	const char* fs_src = GLLoadShaderSource(name, GL_FRAGMENT_SHADER);
	
	GLuint vs = GLCompileShader(name, GL_VERTEX_SHADER, vs_src);
	GLuint fs = GLCompileShader(name, GL_FRAGMENT_SHADER, fs_src);
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	int success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		char error_buffer[2048];
		GLsizei error_length;
		glGetProgramInfoLog(program, sizeof(error_buffer), &error_length, error_buffer);
		Fatal("Failed to link %s:\n\n%s", name, error_buffer);
	}

	glDeleteShader(vs);
	glDeleteShader(fs);

	GL_ERROR_CHECK();
	return program;
}

void GL_ERROR_CHECK_Impl(const char* file, int line, GLenum error)
{
	Fatal("GL_ERROR_CHECK(%s:%d) - Error 0x%x", file, line, error);
}

Mesh* MeshCreate(
	const char* name,
	size_t num_vertices, const MeshVertex* vertices,
	size_t num_indices, const U32* indices)
{
	Mesh* mesh = new Mesh();
	strcpy(mesh->name, name);
	mesh->index_count = num_indices;

	glGenVertexArrays(1, &mesh->vao);
	glBindVertexArray(mesh->vao);

	glGenBuffers(1, &mesh->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(MeshVertex) * num_vertices, vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &mesh->ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * num_indices, indices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(MeshVertex), (void*)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
	glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(MeshVertex), (void*)offsetof(MeshVertex, texcoord));

	return mesh;
}

Texture* TextureCreate(const char* name, int width, int height, const U8* pixels, GLenum format)
{
	Texture* texture = new Texture();
	texture->width = width;
	texture->height = height;

	glGenTextures(1, &texture->handle);
	glBindTexture(GL_TEXTURE_2D, texture->handle);

	GLenum baseformat, type;
	switch (format)
	{
		case GL_RGBA8: baseformat = GL_RGBA; type = GL_UNSIGNED_BYTE; break;
		case GL_R8:    baseformat = GL_RED;  type = GL_UNSIGNED_BYTE; break;
		default: Fatal("TextureCreate: Invalid format");
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, baseformat, type, pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	GL_ERROR_CHECK();
	return texture;
}

Texture* TextureLoad(const char* name)
{
	char path[256];
	snprintf(path, sizeof(path), "%s/Assets/%s", EVA_BASE_DIR, name);

	int width, height, channels_in_file;
	U8* pixels = stbi_load(path, &width, &height, &channels_in_file, 4);
	if (!pixels)
	{
		Fatal("Failed to load %s", path);
	}
	DEFER(free(pixels));

	return TextureCreate(name, width, height, pixels, GL_RGBA8);
}