#include <EVA/Game.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Renderer.hpp>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/quat.h>

Game* ActiveGame = nullptr;
extern int DrawMode;

void GameInit(Game* game, const char* name)
{
	game->name = strdup(name);

	CameraInit(game->camera);
	game->camera.position.y = -30;
	game->camera.position.z = 3;

	EntityManagerInit(game->entity_manager);

	game->physics = PhysicsCreate();
}

void GameTick(Game* game, double dt)
{
	PhysicsTick(game->physics, dt);

	if (ActiveGame == game)
	{
		CameraFly(game->camera);
	}
	CameraUpdateMatrices(game->camera);
}

void GameDraw(Game* game)
{
	switch (DrawMode)
	{
		case 0:
		{
			game->entity_manager.StaticMesh.Iterate(
				[](EStaticMesh* entity)
				{
					if (entity->mesh)
					{
						float4x4 model_matrix;
						glm_translate_make(model_matrix, &entity->position.x);
						glm_quat_rotate(model_matrix, &entity->rotation.x, model_matrix);
						glm_scale(model_matrix, &entity->scale.x);
						DrawMesh(entity->mesh, model_matrix);
					}
				});
			break;
		}
		case 1:
		{
			PhysicsDebugDraw(game->physics);
			break;
		}
	}
}