#include <EVA/GL.hpp>
#include <EVA/GLTF.hpp>
#include <SDL3/SDL.h>
#include <cglm/clipspace/persp_lh_no.h>
#include <cglm/clipspace/view_lh_zo.h>
#include <cglm/mat4.h>

SDL_Window* GameWindow = nullptr;
bool DoQuit = false;

GLTF* gltf_monke = nullptr;
Mesh* mesh_quad = nullptr;

float  FOV            = 60 * GLM_PI / 180.0f;
float3 CameraPosition = {0, 0, 10};
float3 CameraForward  = {0, 0, -1};
float3 CameraUp       = {0, 1, 0};

int WindowWidth = 1600;
int WindowHeight = 900;


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

	GLInit();

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

		SDL_GetWindowSize(GameWindow, &WindowWidth, &WindowHeight);

		glViewport(0, 0, WindowWidth, WindowHeight);
		glClearColor(0, 0, 0, 1);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDepthFunc(GL_LESS);

		Mesh* mesh = gltf_monke->meshes[0];

		vec4 c;
		float4x4 view;
		glm_look_lh_zo(CameraPosition, CameraForward, CameraUp, view);

		float4x4 projection;
		glm_perspective_lh_no(FOV, (float)WindowWidth / (float)WindowHeight, 0.1f, 500.0f, projection);

		float4x4 view_projection;
		glm_mat4_mul(projection, view, view_projection);

		glUseProgram(main_program);
		glUniformMatrix4fv(0, 1, false, (float*)&view_projection);
		glBindVertexArray(mesh->vao);
		glDrawElements(GL_TRIANGLES, mesh->index_count, GL_UNSIGNED_INT, (void*)0);
		GL_ERROR_CHECK();

		SDL_GL_SwapWindow(GameWindow);
	}

	return 0;
}