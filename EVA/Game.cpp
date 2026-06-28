#include <EVA/App.hpp>
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

Game*    g_games[8]        = {};
Game*    g_active_game     = nullptr;

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

			if (!g_games[id])
			{
				g_games[id] = new Game();
				GameInit(g_games[id]);
				g_games[id]->id = id;
			}
			
			AppSetMode(AppMode_Game, g_games[id]);
		},
};

ConVar cvar_show_fps = {
	.name = "show_fps",
	.help = "show fps on screen",
	.fvalue = 0,
};

void Con_tp(ConParser& parser)
{
	g_current_camera->position.x = parser.FloatArg(g_current_camera->position.x);
	g_current_camera->position.y = parser.FloatArg(g_current_camera->position.y);
	g_current_camera->position.z = parser.FloatArg(g_current_camera->position.z);
}

void Con_map(ConParser& parser)
{
	if (!g_active_game)
	{
		ConExec("game 0");
	}
	GameLoadMap(g_active_game, parser.StringArg());
}

void GameInitialize()
{
	ConRegisterVar(&cvar_game);
	ConRegisterVar(&cvar_show_fps);

	ConRegisterCommand("tp", Con_tp, "teleport to a position");
	ConRegisterCommand("map", Con_map, "load a map");
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

	if (g_active_game == game)
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

	DrawSetLayer(Layer_Main);
	for (CSGBrush* brush : game->level_brushes)
	{
		DrawMesh(brush->mesh, Library::mat_brush, float4x4::Identity(), COLOR_WHITE);
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
	for (Game* game : g_games)
	{
		if (game) GameTick(game, dt);
	}
}

void GameUnloadMap(Game* game)
{
	for (CSGBrush* b : game->level_brushes)
	{
		CSGDestroyBrush(b);
	}
	game->level_brushes.clear();
}

void GameLoadMap(Game* game, const char* name)
{
	int n;
	char path[256];
	snprintf(path, 256, "%s/Assets/%s.map", EVA_BASE_DIR, name);
	FILE* f = fopen(path, "rb");
	if (!f)
	{
		ConLog("Failed to open %s", path);
		return;
	}
	DEFER(fclose(f));

	fscanf(f, "type map\n");

	int version;
	n = fscanf(f, "version %d\n", &version);
	assert(n == 1);
	if (version != 1)
	{
		ConError("map %s is version %d, expected %d", name, version, 1);
		return;
	}

	int num_brushes;
	n = fscanf(f, "brushes %d\n", &num_brushes);
	assert(n == 1);

	for (int i = 0; i < num_brushes; i++)
	{
		int num_planes;
		n = fscanf(f, "planes %d\n", &num_planes);
		assert(n == 1);

		CSGBrush* b = CSGCreateBrush();

		for (int j = 0; j < num_planes; j++)
		{
			Plane plane;
			n = fscanf(f, "plane %f %f %f %f\n", PRINT_V3(&plane.normal), &plane.distance);
			assert(n == 4);
			b->planes.push_back({ .plane = plane });
		}
		CSGBuildBrush(b);
		CSGBuildBrushMesh(b);
		game->level_brushes.push_back(b);
	}
}