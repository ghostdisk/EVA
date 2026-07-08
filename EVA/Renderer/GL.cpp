#include <EVA/Renderer/GL.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Assets/Material.hpp>
#include <EVA/Assets/Mesh.hpp>
#include <EVA/Assets/Shader.hpp>
#include <EVA/Platform.hpp>
#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>

SDL_GLContext GL = nullptr;

#define STB_IMAGE_IMPLEMENTATION
#include <Vendor/stb_image.h>

void GLPreInitialize() {
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);
}

void GLInitialize() {
	GL = SDL_GL_CreateContext(g_game_window);
	if (!GL)
		Fatal("SDL_GL_CreateContext: %s", SDL_GetError());

	if (!SDL_GL_MakeCurrent(g_game_window, GL))
		Fatal("SDL_GL_MakeCurrent: %s", SDL_GetError());

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
		Fatal("gladLoadGLLoader failed");
}

static char* GLLoadShaderSource(const char* name, GLenum stage) {
	char path[256];
	snprintf(path, sizeof(path), "%s/EVA/Shaders/%s.%s.glsl", EVA_BASE_DIR, name, (stage == GL_VERTEX_SHADER ? "vs" : "fs"));
	
	void* data;
	if (!ReadEntireFile(path, &data, nullptr))
		Fatal("GLLoadShaderSource: Failed to read %s\n", path);

	return (char*)data;
}

static GLuint GLCompileShader(const char* name, GLenum type, const char* source, const char* preamble) {
	GLuint shader = glCreateShader(type);
	const char* sources[] = { preamble, source };
	glShaderSource(shader, EVA_ARRAYSIZE(sources), sources, nullptr);
	glCompileShader(shader);

	int success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char error_buffer[2048];
		GLsizei error_length;
		glGetShaderInfoLog(shader, sizeof(error_buffer), &error_length, error_buffer);
		Fatal("Failed to compile %s:\n\n%s", name, error_buffer);
	}

	GL_ERROR_CHECK();
	return shader;
}

Shader* GLCompileShader(const char* name, int num_defines, const char** defines) {
	Shader* shader = new Shader();
	AssetInit(shader, name);

	shader->handle = glCreateProgram();

	char* vs_src = GLLoadShaderSource(name, GL_VERTEX_SHADER);
	char* fs_src = GLLoadShaderSource(name, GL_FRAGMENT_SHADER);

	std::vector<char> preamble;

	auto PushString = [&](const char* str) {
		int len = strlen(str);
		int head = preamble.size();
		preamble.resize(head + len);
		memcpy(preamble.data() + head, str, len);
	};

	PushString("#version 430 core\n\n");
	for (int i = 0; i < num_defines; i++) {
		PushString("#define ");
		PushString(defines[i]);
		PushString("\n");
	}
	preamble.push_back('\0');
	
	GLuint vs = GLCompileShader(name, GL_VERTEX_SHADER, vs_src, preamble.data());
	GLuint fs = GLCompileShader(name, GL_FRAGMENT_SHADER, fs_src, preamble.data());
	glAttachShader(shader->handle, vs);
	glAttachShader(shader->handle, fs);
	glLinkProgram(shader->handle);

	int success = 0;
	glGetProgramiv(shader->handle, GL_LINK_STATUS, &success);
	if (!success) {
		char error_buffer[2048];
		GLsizei error_length;
		glGetProgramInfoLog(shader->handle, sizeof(error_buffer), &error_length, error_buffer);
		Fatal("Failed to link %s:\n\n%s", name, error_buffer);
	}

	glDeleteShader(vs);
	glDeleteShader(fs);
	free(vs_src);
	free(fs_src);

	GL_ERROR_CHECK();
	return shader;
}

void GL_ERROR_CHECK_Impl(const char* file, int line, GLenum error) {
	//Fatal("GL_ERROR_CHECK(%s:%d) - Error 0x%x", file, line, error);
	printf("GL_ERROR_CHECK(%s:%d) - Error 0x%x\n", file, line, error);
}

void Mesh::Upload() {
	index_count = indices.size();
	vertex_count = vertices.size();

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(MeshVertex) * vertex_count, vertices.data(), GL_STATIC_DRAW);

	if (indices.size()) {
		glGenBuffers(1, &ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(), GL_STATIC_DRAW);
	}

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(MeshVertex), (void*)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
	glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(MeshVertex), (void*)offsetof(MeshVertex, texcoord));
	GL_ERROR_CHECK();
}

void Mesh::Deinit() {
	if (vbo) { glDeleteBuffers(1, &vbo);      vao = 0; }
	if (ibo) { glDeleteBuffers(1, &ibo);      ibo = 0; }
	if (vao) { glDeleteVertexArrays(1, &vao); vbo = 0; }
}

void Texture::Upload(int width, int height, const U8* pixels, GLenum format) {
	this->width = width;
	this->height = height;

	glGenTextures(1, &handle);
	glBindTexture(GL_TEXTURE_2D, handle);

	GLenum baseformat, type;
	switch (format) {
		case GL_RGBA8: baseformat = GL_RGBA; type = GL_UNSIGNED_BYTE; break;
		case GL_R8:    baseformat = GL_RED;  type = GL_UNSIGNED_BYTE; break;
		default: Fatal("TextureCreate: Invalid format");
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, baseformat, type, pixels);

	if (props.generate_mipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);

	switch (props.interpolation) {
		case TextureInterpolation::Point: {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, props.generate_mipmaps ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			break;
		}
		case TextureInterpolation::Bilinear: {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, props.generate_mipmaps ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			break;
		}
		case TextureInterpolation::Trilinear: {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, props.generate_mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			break;
		}
	}

	GLenum mode = GL_REPEAT;
	switch (props.wrap_mode) {
		case TextureWrapMode::Repeat:         mode = GL_REPEAT; break;
		case TextureWrapMode::MirroredRepeat: mode = GL_MIRRORED_REPEAT; break;
		case TextureWrapMode::Clamp:          mode = GL_CLAMP_TO_EDGE; break;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode);
	GL_ERROR_CHECK();
}

Material* MaterialCreate(const char* name, Shader* shader, Texture* texture) {
	Material* material = new Material();
	AssetInit(material, name);

	material->shader = shader;
	material->color_texture = texture;
	return material;
}
