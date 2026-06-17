#include <EVA/Game.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Renderer.hpp>
#include <EVA/GLTF.hpp>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/quat.h>
#include <tracy/Tracy.hpp>

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
	ZoneScopedN("GameTick");
	PhysicsTick(game->physics, dt);

	if (ActiveGame == game)
	{
		CameraFly(game->camera);
	}
	CameraUpdateMatrices(game->camera);
}

void GameDraw(Game* game)
{
	ZoneScopedN("GameDraw");
	switch (DrawMode)
	{
		case 0:
		{
			game->entity_manager.Iterate(
				[](Entity* entity)
				{
					if (entity->mesh)
					{
						float4x4 model_matrix;
						glm_translate_make(model_matrix, &entity->position.x);
						glm_quat_rotate(model_matrix, &entity->rotation.x, model_matrix);
						glm_scale(model_matrix, &entity->scale.x);
						DrawMesh(entity->mesh, entity->material, model_matrix);
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

EID InstantiateScene(Game* game, GLTFScene* scene, EID start_eid)
{
	EID eid = start_eid;

	for (const GLTFSceneNode& node : scene->nodes)
	{
		if (node.mesh)
		{
			EStaticMesh* entity = game->entity_manager.StaticMesh.CreateEntity(eid++);
			entity->mesh     = node.mesh;
			entity->material = node.material;
			entity->position = node.position;
			entity->rotation = node.rotation;
			entity->scale    = node.scale;

			if (node.mesh->collider)
			{
				PhysicsAttachBodyToEntity(game->physics, entity, node.mesh->collider, PhysicsLayer_NonMoving);
			}
		}
	}

	return eid;
}