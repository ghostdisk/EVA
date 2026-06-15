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

	float triangle[] = {
		-0.5, -0.5, 0,
		0.5, -0.5, 0,
		0, 0.5, 0,
	};

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangle), &triangle, GL_STATIC_DRAW);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 12, (void*)0);

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
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		GL_ERROR_CHECK();

		SDL_GL_SwapWindow(GameWindow);
	}

	return 0;
}