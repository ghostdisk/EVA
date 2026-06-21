#include <EVA/Game.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Renderer.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/CSG.hpp>
#include <EVA/IO.hpp>
#include <EVA/UI.hpp>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/quat.h>
#include <tracy/Tracy.hpp>

Game* ActiveGame = nullptr;
CSGStack* stack;
std::vector<CSGBrush*> draw_brushes;
int k = 0;

void GameInit(Game* game, const char* name)
{
	game->name = strdup(name);

	CameraInit(game->camera);
	game->camera.position.y = -10;
	game->camera.position.z = 3;

	EntityManagerInit(game->entity_manager);

	game->physics = PhysicsCreate();


#if 0
	CSGBrush* cube1 = CSGCreateCube({2,1,1});
	CSGBuildBrush(cube1);
	CSGBuildBrushMesh(cube1);

	CSGBrush* cube2 = CSGCloneBrush(cube1);
	cube2->planes.push_back({ .plane = Plane({-1,0,0}, -2.5) });
	CSGBuildBrush(cube2);
	CSGBuildBrushMesh(cube2);

	// CSGBrush* cube3 = CSGCloneBrush(cube1);
	// cube3->planes.push_back({ .plane = Plane({-1,0,0}, 1.5).Invert() });
	// CSGBuildBrush(cube3);
	// CSGBuildBrushMesh(cube3);

	// draw_brushes.push_back(cube1);
	draw_brushes.push_back(cube2);
	// draw_brushes.push_back(cube3);
#else
	stack = CSGCreateStack();
	stack->nodes.push_back(CSGStackNode{
		.type      = CSGStackNodeType_Brush,
		.operation = CSGOperation_Union,
		.brush     = CSGCreateCube({1,1,1}),
	});
	stack->nodes.push_back(CSGStackNode{
		.type      = CSGStackNodeType_Brush,
		.operation = CSGOperation_Difference,
		.brush     = CSGCreateCube({2,.5,.5}),
	});
	stack->nodes.push_back(CSGStackNode{
		.type      = CSGStackNodeType_Brush,
		.operation = CSGOperation_Difference,
		.brush     = CSGCreateCube({.5,.5,2}),
	});
	stack->nodes.push_back(CSGStackNode{
		.type      = CSGStackNodeType_Brush,
		.operation = CSGOperation_Difference,
		.brush     = CSGCreateCube({.5, 2, .5}),
	});
	CSGBuildStack(stack);
	for (CSGBrush* b : stack->built_brushes)
	{
		CSGBuildBrushMesh(b);
	}
	draw_brushes = stack->built_brushes;
#endif
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
	CSGDrawInspector(main_ui, stack);

	float4 colors[] = {
		{ 1, 0, 0, 1 },
		{ 0, 1, 0, 1 },
		{ 0, 0, 1, 1 },
		{ 1, 1, 0, 1 },
		{ 0, 1, 1, 1 },
		{ 1, 0, 1, 1 },
		{ 1, 1, 1, 1 },
	};

	for (int i = 0; i < draw_brushes.size(); i++)
	{
		if (1 || i == (k % draw_brushes.size()))
		{
			// printf("Drawing brush %d\n", (int)(k % draw_brushes.size()));
			DrawMesh(draw_brushes[i]->mesh, nullptr, float4x4::Identity(), colors[i % EVA_ARRAYSIZE(colors)]);
		}
	}
	if (IOGetButtonDown(SDL_SCANCODE_K)) k++;

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