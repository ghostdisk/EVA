#include <EVA/Renderer/GL.hpp>
#include <EVA/Asset.hpp>
#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>

SDL_GLContext GL = nullptr;

#define STB_IMAGE_IMPLEMENTATION
#include <Vendor/stb_image.h>

void GLInitialize()
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
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

static char* GLLoadShaderSource(const char* name, GLenum stage)
{
	char path[256];
	snprintf(path, sizeof(path), "%s/EVA/Shaders/%s.%s.glsl", EVA_BASE_DIR, name, (stage == GL_VERTEX_SHADER ? "vs" : "fs"));
	
	void* data;
	if (!ReadEntireFile(path, &data, nullptr))
	{
		Fatal("GLLoadShaderSource: Failed to read %s\n", path);
	}

	return (char*)data;
}

static GLuint GLCompileShader(const char* name, GLenum type, const char* source, const char* preamble)
{
	GLuint shader = glCreateShader(type);
	const char* sources[] = { preamble, source };
	glShaderSource(shader, EVA_ARRAYSIZE(sources), sources, nullptr);
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

GLuint GLCompileShaderProgram(const char* name, int num_defines, const char** defines)
{
	GLuint program = glCreateProgram();

	char* vs_src = GLLoadShaderSource(name, GL_VERTEX_SHADER);
	char* fs_src = GLLoadShaderSource(name, GL_FRAGMENT_SHADER);

	std::vector<char> preamble;

	auto PushString = [&](const char* str)
	{
		int len = strlen(str);
		int head = preamble.size();
		preamble.resize(head + len);
		memcpy(preamble.data() + head, str, len);
	};

	PushString("#version 430 core\n\n");
	for (int i = 0; i < num_defines; i++)
	{
		PushString("#define ");
		PushString(defines[i]);
		PushString("\n");
	}
	preamble.push_back('\0');
	
	GLuint vs = GLCompileShader(name, GL_VERTEX_SHADER, vs_src, preamble.data());
	GLuint fs = GLCompileShader(name, GL_FRAGMENT_SHADER, fs_src, preamble.data());
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
	free(vs_src);
	free(fs_src);

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
	AssetInit(mesh, AssetType_Mesh, name);

	mesh->index_count = num_indices;
	mesh->vertex_count = num_vertices;

	glGenVertexArrays(1, &mesh->vao);
	glBindVertexArray(mesh->vao);

	glGenBuffers(1, &mesh->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(MeshVertex) * num_vertices, vertices, GL_STATIC_DRAW);

	if (indices)
	{
		glGenBuffers(1, &mesh->ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * num_indices, indices, GL_STATIC_DRAW);
	}

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(MeshVertex), (void*)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
	glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(MeshVertex), (void*)offsetof(MeshVertex, texcoord));

	return mesh;
}

void MeshDestroy(Mesh* mesh)
{
	if (mesh->vbo) glDeleteBuffers(1, &mesh->vbo);
	if (mesh->ibo) glDeleteBuffers(1, &mesh->ibo);
	if (mesh->vao) glDeleteVertexArrays(1, &mesh->vao);
	AssetDeinit(mesh);
	delete mesh;
}

Texture* TextureCreate(const char* name, int width, int height, const U8* pixels, GLenum format, bool mips)
{
	Texture* texture = new Texture();
	texture->width = width;
	texture->height = height;
	AssetInit(texture, AssetType_Texture, name);

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

	if (mips)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	GL_ERROR_CHECK();
	return texture;
}

Texture* TextureLoad(const char* name, bool mips)
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

	char name_without_ext[64];
	int len = snprintf(name_without_ext, 64, "%s", name);
	ReplaceFileExtension(name_without_ext, 64, "");

	return TextureCreate(name_without_ext, width, height, pixels, GL_RGBA8, mips);
}

Material* MaterialCreate(const char* name, GLuint shader, Texture* texture)
{
	Material* material = new Material();
	AssetInit(material, AssetType_Material, name);

	material->shader = shader;
	material->color_texture = texture;
	return material;
}
