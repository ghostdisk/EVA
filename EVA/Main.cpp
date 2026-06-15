#include <EVA/GL.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/IO.hpp>
#include <EVA/Camera.hpp>
#include <SDL3/SDL.h>
#include <cglm/mat4.h>

SDL_Window* GameWindow = nullptr;
bool DoQuit = false;

GLTF* gltf_monke = nullptr;
Mesh* mesh_quad = nullptr;

int WindowWidth = 1600;
int WindowHeight = 900;

Camera camera;

// time:
static U64 FrameStartTimeNS;
double DeltaTime = 0.01;

int main()
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		Fatal("SDL_Init: %s", SDL_GetError());
	}

	GameWindow = SDL_CreateWindow("EVA", WindowWidth, WindowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!GameWindow)
	{
		Fatal("SDL_CreateWindow: %s", SDL_GetError());
	}

	GLInitialize();
	IOInitialize();

	GLuint main_program = GLCompileShaderProgram("Main");

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

	CameraInit(camera);
	camera.position.y = -10;

	while (!DoQuit)
	{
		U64 new_time = SDL_GetTicksNS();
		U64 dt_ns = new_time - FrameStartTimeNS;
		DeltaTime = double(dt_ns) / 1'000'000'000;
		FrameStartTimeNS = new_time;

		IOBeginFrame();

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			IOHandleSDLEvent(&event);
			switch (event.type)
			{
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				{
					DoQuit = true;
					break;
				}
			}
		}

		{ // Process input:
		}

		{ // Simulate game:
			CameraFly(camera);
			CameraUpdateMatrices(camera);
		}

		{ // Render frame:
			SDL_GetWindowSize(GameWindow, &WindowWidth, &WindowHeight);

			glViewport(0, 0, WindowWidth, WindowHeight);
			glClearColor(0, 0, 0, 1);
			glEnable(GL_DEPTH_TEST);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDepthFunc(GL_LESS);

			Mesh* mesh = gltf_monke->meshes[0];

			glUseProgram(main_program);
			glUniformMatrix4fv(0, 1, false, (float*)&camera.view_projection_matrix);
			glBindVertexArray(mesh->vao);
			glDrawElements(GL_TRIANGLES, mesh->index_count, GL_UNSIGNED_INT, (void*)0);
			GL_ERROR_CHECK();

			SDL_GL_SwapWindow(GameWindow);
		}

	}

	return 0;
}