#include <EVA/GL.hpp>
#include <SDL3/SDL.h>

SDL_Window* GameWindow = nullptr;
bool DoQuit = false;

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

	MeshVertex quad_vertices[] = {
		MeshVertex { .position = float3(0, 0, 0) },
		MeshVertex { .position = float3(1, 0, 0) },
		MeshVertex { .position = float3(1, 1, 0) },
		MeshVertex { .position = float3(0, 1, 0) },
	};
	U32 quad_indices[] = { 0, 1, 2, 0, 2, 3 };

	Mesh* mesh_quad = CreateMesh("mesh_quad",
		EVA_ARRAYSIZE(quad_vertices), quad_vertices,
		EVA_ARRAYSIZE(quad_indices), quad_indices);

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

		glUseProgram(triangle_program);
		glBindVertexArray(mesh_quad->vao);
		glDrawElements(GL_TRIANGLES, mesh_quad->index_count, GL_UNSIGNED_INT, (void*)0);
		GL_ERROR_CHECK();

		SDL_GL_SwapWindow(GameWindow);
	}

	return 0;
}