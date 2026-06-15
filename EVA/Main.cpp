#include <EVA/GL.hpp>
#include <EVA/GLTF.hpp>
#include <SDL3/SDL.h>

SDL_Window* GameWindow = nullptr;
bool DoQuit = false;

GLTF* gltf_monke = nullptr;
Mesh* mesh_quad = nullptr;


int main()
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		Fatal("SDL_Init: %s", SDL_GetError());
	}

	GameWindow = SDL_CreateWindow("EVA", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!GameWindow)
	{
		Fatal("SDL_CreateWindow: %s", SDL_GetError());
	}

	GLInit();

	GLuint triangle_program = GLCompileShaderProgram("Triangle");

	{ // quad mesh:
		MeshVertex quad_vertices[] = {
			MeshVertex { .position = float3(0, 0, 0) },
			MeshVertex { .position = float3(1, 0, 0) },
			MeshVertex { .position = float3(1, 1, 0) },
			MeshVertex { .position = float3(0, 1, 0) },
		};
		U32 quad_indices[] = { 0, 1, 2, 0, 2, 3 };
		mesh_quad = CreateMesh("mesh_quad",
			EVA_ARRAYSIZE(quad_vertices), quad_vertices,
			EVA_ARRAYSIZE(quad_indices), quad_indices);
	}

	{ // monke gltf:
		gltf_monke = GLTFLoad("monke");
	}

	while (!DoQuit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				{
					DoQuit = true;
					break;
				}
			}
		}

		glClearColor(1, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		Mesh* mesh = gltf_monke->meshes[0];

		glUseProgram(triangle_program);
		glBindVertexArray(mesh->vao);
		glDrawElements(GL_TRIANGLES, mesh->index_count, GL_UNSIGNED_INT, (void*)0);
		GL_ERROR_CHECK();

		SDL_GL_SwapWindow(GameWindow);
	}

	return 0;
}