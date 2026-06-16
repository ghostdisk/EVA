#include <EVA/GL.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/IO.hpp>
#include <EVA/UI.hpp>
#include <EVA/Draw.hpp>
#include <EVA/Camera.hpp>
#include <SDL3/SDL.h>
#include <cglm/mat4.h>

SDL_Window* GameWindow = nullptr;
bool DoQuit = false;

GLTF* gltf_monke = nullptr;
Texture* tex_test = nullptr;

int WindowWidth = 1600;
int WindowHeight = 900;

Camera camera;
UIContext UI;
DrawContext DC;
Font* fnt_arial = 0;

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
	DrawInitialize();
	UIInitialize();

	fnt_arial = FontLoad("Arial.ttf", 24, 512);

	DrawContextInit(DC);
	UIContextInit(UI, fnt_arial);

	GLuint main_program = GLCompileShaderProgram("Main");


	{ // load assets:
		gltf_monke = GLTFLoad("monke.glb");
		tex_test = TextureLoad("test.jpg");
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

		UIBeginFrame(UI);
		UI.root.flex_axis = 1;

		{ // Process input:
		}

		{ // Simulate game:
			CameraFly(camera);
			CameraUpdateMatrices(camera);

			// Dummy UI:
			UISetPadding(&UI.root, 8);
			UISetGap(&UI.root, 8);
			{
				if (UIButton(UI, "Button 1")) { printf("Button 1 was pressed"); }
				if (UIButton(UI, "Button 2")) { printf("Button 2 was pressed"); }
				if (UIButton(UI, "Button 3")) { printf("Button 3 was pressed"); }
				if (UIButton(UI, "Button 4")) { printf("Button 4 was pressed"); }
			}
		}

		UIEndFrame(UI);
		UIDraw(UI, DC);

		{ // Render frame:
			SDL_GetWindowSize(GameWindow, &WindowWidth, &WindowHeight);

			glViewport(0, 0, WindowWidth, WindowHeight);
			glClearColor(0.2, 0.2, 0.2, 1);

			glEnable(GL_DEPTH_TEST);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDepthFunc(GL_LESS);
			glDisable(GL_BLEND);

			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);

			{ // render a mesh:
				Mesh* mesh = gltf_monke->meshes[0];

				glBindTexture(GL_TEXTURE_2D, tex_test->handle);
				glActiveTexture(GL_TEXTURE0);
				GL_ERROR_CHECK();

				glUseProgram(main_program);
				glUniform1i(1, 0);
				GL_ERROR_CHECK();

				glUniformMatrix4fv(0, 1, false, (float*)&camera.view_projection_matrix);
				glBindVertexArray(mesh->vao);
				glDrawElements(GL_TRIANGLES, mesh->index_count, GL_UNSIGNED_INT, (void*)0);
			}

			{ // render ui:
				DrawRender(DC);
			}

			GL_ERROR_CHECK();

			SDL_GL_SwapWindow(GameWindow);
		}
	}

	return 0;
}