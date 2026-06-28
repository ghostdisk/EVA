#include <EVA/Game.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Library.hpp>
#include <EVA/Console.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/quat.h>
#include <tracy/Tracy.hpp>

Game* games[8] = {};
Game* ActiveGame = nullptr;

ConVar cvar_game = {
	.name = "game",
	.help = "set active game (0 to 7)",
	.fvalue = 0,
	.on_change =
		[](ConVar* v)
		{
			int id = (int)cvar_game.fvalue;
			if (id < 0) id = 0;
			if (id > 7) id = 7;
			cvar_game.fvalue = id;

			if (!games[id])
			{
				games[id] = new Game();
				GameInit(games[id]);
				games[id]->id = id;
			}
			ActiveGame = games[id];
		},
};

ConVar cvar_show_fps = {
	.name = "show_fps",
	.help = "show fps on screen",
	.fvalue = 0,
};

void Con_tp(ConParser& parser)
{
	Camera* cam = &ActiveGame->camera;

	cam->position.x = parser.FloatArg(cam->position.x);
	cam->position.y = parser.FloatArg(cam->position.y);
	cam->position.z = parser.FloatArg(cam->position.z);
}

void GameInitialize()
{
	ConRegisterVar(&cvar_game);
	ConRegisterVar(&cvar_show_fps);
	ConRegisterCommand("tp", Con_tp, "teleport to a position");
}

void GameInit(Game* game)
{
	CameraInit(game->camera);
	game->camera.position.y = -10;
	game->camera.position.z = 3;

	EntityManagerInit(game->entity_manager);
}

void GameTick(Game* game, double dt)
{
	ZoneScopedN("GameTick");

	if (ActiveGame == game)
	{
		if (!game->pawn)
		{
			CameraFly(game->camera);
		}
	}
	CameraUpdateMatrices(game->camera);
}

void GameDraw(Game* game)
{
	ZoneScopedN("GameDraw");

	game->entity_manager.Iterate(
		[](Entity* entity)
		{
			if (entity->mesh)
			{
				float4x4 model_matrix;
				glm_translate_make(model_matrix, &entity->position.x);
				glm_quat_rotate(model_matrix, &entity->rotation.x, model_matrix);
				glm_scale(model_matrix, &entity->scale.x);
				DrawMesh(entity->mesh, entity->material, model_matrix, {1,1,1,1});
			}
		});
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
			EntitySetName(entity, node.name);
		}
	}

	return eid;
}

void GameTickAll(double dt)
{
	for (Game* game : games)
	{
		if (game) GameTick(game, dt);
	}
}