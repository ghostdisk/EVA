#include <EVA/GL.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/IO.hpp>
#include <EVA/UI.hpp>
#include <EVA/Draw.hpp>
#include <EVA/Camera.hpp>
#include <EVA/Arena.hpp>
#include <EVA/Entities.hpp>
#include <EVA/Renderer.hpp>
#include <SDL3/SDL.h>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/quat.h>

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

float FrameTimeHistory[50] = {};
float FPS = 0;
bool VSync = false;

// time:
static U64 FrameStartTimeNS;
double DeltaTime = 0.01;

EntityManager entity_manager;

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

	ArenaInitialize();
	GLInitialize();
	RendererInitialize();
	IOInitialize();
	DrawInitialize();
	UIInitialize();

	fnt_arial = FontLoad("Arial.ttf", 16, 512);

	DrawContextInit(DC);
	UIContextInit(UI, fnt_arial);

	{ // load assets:
		gltf_monke = GLTFLoad("monke.glb");
		tex_test = TextureLoad("test.jpg");
	}

	{ // Create dumm yentitiy world:

		EntityManagerInit(entity_manager);
		EStaticMesh* monkey1 = entity_manager.StaticMesh.CreateEntity(1);
		EStaticMesh* monkey2 = entity_manager.StaticMesh.CreateEntity(1);
		monkey1->mesh = gltf_monke->meshes[0];
		monkey2->mesh = gltf_monke->meshes[0];

		monkey1->position.x = 4;
	}

	GLuint main_program = GLCompileShaderProgram("Main");

	CameraInit(camera);
	camera.position.y = -10;

	while (!DoQuit)
	{
		U64 new_time = SDL_GetTicksNS();
		U64 dt_ns = new_time - FrameStartTimeNS;
		DeltaTime = double(dt_ns) / 1'000'000'000;
		FrameStartTimeNS = new_time;


		RotateFrameArenas();
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
		UISetPadding(&UI.root, 8);
		UISetGap(&UI.root, 8);

		{ // Update tracked FPS
			static int k = 0;
			FrameTimeHistory[k] = DeltaTime;
			k++;
			if (k >= EVA_ARRAYSIZE(FrameTimeHistory)) k = 0;

			float avg = 0;
			for (int i = 0; i < EVA_ARRAYSIZE(FrameTimeHistory); i++)
			{
				avg += FrameTimeHistory[i];
			}
			avg = avg / (float)EVA_ARRAYSIZE(FrameTimeHistory);
			FPS = 1.0f / avg;
		}

		{ // Simulate game:
			CameraFly(camera);
			CameraUpdateMatrices(camera);

			// FPS counter
			{
				char fps_value[64];
				snprintf(fps_value, 64, "FPS: %f", FPS);
				UILabel(UI, fps_value);
			}

			char buf[64];
			snprintf(buf, 64, "Toggle VSync (%s)", VSync ? "on" : "off");
			if (UIButton(UI, buf))
			{
				VSync = !VSync;
				SDL_GL_SetSwapInterval(VSync ? 1 : 0);
			}
		}

		DrawGrid(50);
		DrawLine({0,0,0}, {1,0,0}, {1,0,0,1});
		DrawLine({0,0,0}, {0,1,0}, {0,1,0,1});
		DrawLine({0,0,0}, {0,0,1}, {0,0,1,1});

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

			{ // render mesh entities
				glUseProgram(main_program);
				glUniformMatrix4fv(0, 1, false, (float*)&camera.view_projection_matrix);

				entity_manager.StaticMesh.Iterate(
					[](EStaticMesh* entity)
					{
						glBindTexture(GL_TEXTURE_2D, tex_test->handle);
						glActiveTexture(GL_TEXTURE0);
						GL_ERROR_CHECK();
						glUniform1i(1, 0);
						GL_ERROR_CHECK();

						float4x4 model_matrix;
						glm_translate_make(model_matrix, &entity->position.x);
						glm_quat_rotate(model_matrix, &entity->rotation.x, model_matrix);
						glm_scale(model_matrix, &entity->scale.x);
						glUniformMatrix4fv(2, 1, false, (float*)&model_matrix);

						glBindVertexArray(entity->mesh->vao);
						glDrawElements(GL_TRIANGLES, entity->mesh->index_count, GL_UNSIGNED_INT, (void*)0);
					});

			}

			{ 
				RenderPendingLines();
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