#include <EVA/App.hpp>
#include <EVA/Game.hpp>
#include <EVA/GameServer.hpp>
#include <EVA/GameClient.hpp>
#include <EVA/Renderer/GL.hpp>
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
#include <vector>

Game* g_games[8]        = {};
Game* g_active_game     = nullptr;

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
				g_games[id]->Init();
				g_games[id]->id = id;
			}
			
			ScreenLog("Game %d", id);
			AppSetMode(AppMode_Game, g_games[id]);
		},
};

ConVar cvar_show_fps = {
	.name = "show_fps",
	.help = "show fps on screen",
	.fvalue = 0,
};

Result Con_tp(ConParser& parser) {
	g_current_camera->position.x = parser.FloatArg(g_current_camera->position.x);
	g_current_camera->position.y = parser.FloatArg(g_current_camera->position.y);
	g_current_camera->position.z = parser.FloatArg(g_current_camera->position.z);
	return Success();
}

Result Con_map(ConParser& parser) {
	const char* map_name = parser.StringArg();
	if (g_app_mode == AppMode_Editor) {
		char ed_load_cmd_buf[64];
		snprintf(ed_load_cmd_buf, 64, "ed_load %s", map_name);
		return ConExec(ed_load_cmd_buf);
	} else {
		if (!g_active_game)
			ConExec("game 0");
		g_active_game->LoadMap(map_name);
	}
	return Success();
}

void GameInitialize() {
	ConRegisterVar(&cvar_game);
	ConRegisterVar(&cvar_show_fps);

	ConRegisterCommand("tp", Con_tp, "teleport to a position");
	ConRegisterCommand("map", Con_map, "load a map");
}

void Game::Init() {
	camera = new ECamera();
	camera->eid = EID_DefaultCamera;
	CameraInit(*camera);
	camera->position.y = -10;
	camera->position.z = 3;
	EntityManagerInit(entity_manager);

	b3WorldDef world_def = b3DefaultWorldDef();
	world_def.gravity = b3Vec3{ 0, 0, -10 };
	physics = b3CreateWorld(&world_def);

	{ // ground:
		b3BodyDef ground_body_def = b3DefaultBodyDef();
		b3BodyId ground_body = b3CreateBody(physics, &ground_body_def);
		b3BoxHull ground_hull = b3MakeBoxHull(10, 10, 1);
		b3ShapeDef ground_shape_def = b3DefaultShapeDef();
		b3ShapeId ground_shape = b3CreateHullShape(ground_body, &ground_shape_def, &ground_hull.base);
	}
}

void Game::Tick(double dt) {
	ZoneScopedN("GameTick");

	if (server) server->Tick(dt);
	if (client) client->Tick(dt);

	if (g_active_game == this && !pawn) {
		CameraFly(*camera);
	}
	CameraUpdateMatrices(*camera);
}

void Game::Draw() {
	ZoneScopedN("Game::Draw");

	DrawSetLayer(Layer_Main);

	static float3 test1 = {};
	if (InputGetButton(SDL_SCANCODE_X)) test1 = g_current_camera->position;

	if (level_mesh) {
		DrawMesh(level_mesh, Library::mat_brush, float4x4::Identity());
	}

	entity_manager.Iterate([](Entity* entity) {
		if (entity->mesh) {
			float4x4 model_matrix;
			glm_translate_make(model_matrix, &entity->position.x);
			glm_quat_rotate(model_matrix, &entity->rotation.x, model_matrix);
			glm_scale(model_matrix, &entity->scale.x);
			DrawMesh(entity->mesh, entity->material, model_matrix, {1,1,1,1});
		}
	});
}

void Game::TickAll(double dt) {
	for (Game* game : g_games) {
		if (!game) continue;
		game->Tick(dt);
	}
}

void Game::UnloadMap() {
	if (level_mesh) {
		MeshDestroy(level_mesh);
		level_mesh = nullptr;
	}
}

Result Game::LoadMap(String name) {
	int n;
	char path[256];
	snprintf(path, 256, "%s/Assets/%.*s.map", EVA_BASE_DIR, STRING_PRINTF_ARGS(name));
	FILE* f = fopen(path, "rb");
	if (!f) return Err("Failed to open %s", path);
	DEFER(fclose(f));
	fscanf(f, "type map\n");

	int version;
	n = fscanf(f, "version %d\n", &version);
	assert(n == 1);
	if (version != 1) return Err("map %s is version %d, expected %d", name, version, 1);

	int num_vertices;
	n = fscanf(f, "vertices %d", &num_vertices);
	assert(n == 1);

	std::vector<MeshVertex> vertices(num_vertices);
	for (int i = 0; i < num_vertices; i++) {
		MeshVertex& vert = vertices[i];
		vert.texcoord = {};
		n = fscanf(f, "%f %f %f %f %f %f", XYZ(&vert.position), XYZ(&vert.normal));
		assert(n == 6);
	}

	int num_indices;
	n = fscanf(f, "\nindices %d", &num_indices);
	assert(n == 1);

	std::vector<U32> indices(num_indices);
	for (int i = 0; i < num_indices; i++) {
		n = fscanf(f, "%u", &indices[i]);
		assert(n == 1);
	}
	fscanf(f, "\n");

	level_mesh = MeshCreate("level_mesh", num_vertices, vertices.data(), num_indices, indices.data());

	int num_entities;
	n = fscanf(f, "entities %d\n", &num_entities);
	if (n != 1) return Err("failed to load map");
	assert(num_entities >= 0 && num_entities < 1000);

	for (int i = 0; i < num_entities; i++) {
		Entity* entity = EntityLoad(&entity_manager, f);
	}

	snprintf(map_name, sizeof(map_name), "%.*s", STRING_PRINTF_ARGS(name));
	return Success();
}