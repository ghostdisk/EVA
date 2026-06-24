#include <EVA/Game.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/GLTF.hpp>
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

ConVar con_game = {
	.name = "game",
	.help = "set active game (0 to 7)",
	.value = ConValue{
		.type = ConValueType_Number,
		.number = 0,
	},
	.on_change =
		[](ConVar* v)
		{
			int id = (int)con_game.value.number;
			if (id < 0) id = 0;
			if (id > 7) id = 7;
			con_game.value.number = id;

			if (!games[id])
			{
				games[id] = new Game();
				GameInit(games[id]);
				games[id]->id = id;
			}
			ActiveGame = games[id];
		},
};

ConValue Con_tp(int argc, ConValue* args)
{
	if (argc >= 1 && args[0].type == ConValueType_Number) ActiveGame->camera.position.x = args[0].number;
	if (argc >= 2 && args[1].type == ConValueType_Number) ActiveGame->camera.position.y = args[1].number;
	if (argc >= 3 && args[2].type == ConValueType_Number) ActiveGame->camera.position.z = args[2].number;
	return {};
}

void GameInitialize()
{
	ConRegisterVar(&con_game);
	ConRegisterCommand("tp", Con_tp, "teleport to a position");
}

void GameInit(Game* game)
{
	CameraInit(game->camera);
	game->camera.position.y = -10;
	game->camera.position.z = 3;

	EntityManagerInit(game->entity_manager);

	game->physics = PhysicsCreate();

	game->csg = CSGCreateStack();

	game->csg->nodes.push_back(CSGStackNode{
		.type      = CSGStackNodeType_Brush,
		.operation = CSGOperation_Union,
		.brush     = CSGCreateCube({10,10,1}),
	});
	/*
	game->csg->nodes.push_back(CSGStackNode{
		.type      = CSGStackNodeType_Brush,
		.transform = float4x4({
			1,0,0,0,
			0,1,0,0,
			0,0,1,0,
			2,0,0,1,
		}),
		.operation = CSGOperation_Difference,
		.brush     = CSGCreateCylinder(32, 1, 2)
	});
	*/

	if (1)
	{
		ZoneScopedN("CSG Rebuild");
		CSGBuildStack(game->csg);
		for (CSGBrush* b : game->csg->built_brushes)
		{
			CSGBuildBrushMesh(b);
		}
	}
}

void GameTick(Game* game, double dt)
{
	ZoneScopedN("GameTick");

	PhysicsTick(game->physics, dt);

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

	float4 colors[] = {
		{ 0.910f, 0.450f, 0.450f, 1.0f },  // red
		{ 0.910f, 0.542f, 0.450f, 1.0f },
		{ 0.910f, 0.634f, 0.450f, 1.0f },
		{ 0.910f, 0.726f, 0.450f, 1.0f },
		{ 0.910f, 0.818f, 0.450f, 1.0f },
		{ 0.910f, 0.910f, 0.450f, 1.0f },  // yellow
		{ 0.818f, 0.910f, 0.450f, 1.0f },
		{ 0.726f, 0.910f, 0.450f, 1.0f },
		{ 0.634f, 0.910f, 0.450f, 1.0f },
		{ 0.542f, 0.910f, 0.450f, 1.0f },
		{ 0.450f, 0.910f, 0.450f, 1.0f },  // green
		{ 0.450f, 0.910f, 0.542f, 1.0f },
		{ 0.450f, 0.910f, 0.634f, 1.0f },
		{ 0.450f, 0.910f, 0.726f, 1.0f },
		{ 0.450f, 0.910f, 0.818f, 1.0f },
		{ 0.450f, 0.910f, 0.910f, 1.0f },  // cyan
		{ 0.450f, 0.818f, 0.910f, 1.0f },
		{ 0.450f, 0.726f, 0.910f, 1.0f },
		{ 0.450f, 0.634f, 0.910f, 1.0f },
		{ 0.450f, 0.542f, 0.910f, 1.0f },
		{ 0.450f, 0.450f, 0.910f, 1.0f },  // blue
		{ 0.542f, 0.450f, 0.910f, 1.0f },
		{ 0.634f, 0.450f, 0.910f, 1.0f },
		{ 0.726f, 0.450f, 0.910f, 1.0f },
		{ 0.818f, 0.450f, 0.910f, 1.0f },
		{ 0.910f, 0.450f, 0.910f, 1.0f },  // magenta
		{ 0.910f, 0.450f, 0.818f, 1.0f },
		{ 0.910f, 0.450f, 0.726f, 1.0f },
		{ 0.910f, 0.450f, 0.634f, 1.0f },
		{ 0.910f, 0.450f, 0.542f, 1.0f },
	};

	DrawSetLayer(Layer_Main);
	for (int i = 0; i < game->csg->built_brushes.size(); i++)
	{
		DrawMesh(game->csg->built_brushes[i]->mesh, nullptr, float4x4::Identity(), colors[i % EVA_ARRAYSIZE(colors)]);
	}

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