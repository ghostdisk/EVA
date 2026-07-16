#include <EVA/App.hpp>
#include <EVA/Game.hpp>
#include <EVA/GameServer.hpp>
#include <EVA/GameClient.hpp>
#include <EVA/GFX/Renderer.hpp>
#include <EVA/Assets/Mesh.hpp>
#include <EVA/GameModes/GameMode.hpp>
#include <EVA/Library.hpp>
#include <EVA/Console.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/quat.h>
#include <tracy/Tracy.hpp>

Game* g_games[8]        = {};
Game* g_active_game     = nullptr;

class Editor;

ConVar cvar_game = {
	.name = "game",
	.help = "set active game (0 to 7)",
	.fvalue = 0,
	.on_change = [](ConVar* v) {
		int id = (int)cvar_game.fvalue;
		if (id < 0) id = 0;
		if (id > 7) id = 7;
		cvar_game.fvalue = id;

		if (!g_games[id]) {
			g_games[id] = new Game();
			g_games[id]->Init();
			g_games[id]->id = id;
		}
		
		g_active_game = g_games[id];
		ScreenLog("Game %d", id);
	},
};

ConVar cvar_show_fps = {
	.name = "show_fps",
	.help = "show fps on screen",
	.fvalue = 0,
};

Result Con_tp(ConParser& parser) {
	ECamera* camera = g_active_game->m_activeCamera;

	camera->position.x = parser.FloatArg(camera->position.x);
	camera->position.y = parser.FloatArg(camera->position.y);
	camera->position.z = parser.FloatArg(camera->position.z);
	return Success();
}

Result Con_map(ConParser& parser) {
	ScratchArena scratch;
	ZTString map_name = parser.StringArg(scratch);

	GameMode* gm = GameMode::GetCurrent();
	if (gm) {
		return gm->LoadMap(map_name);
	} else {
		return Err("no active gamemode");
	}
}

void GameInitialize() {
	ZoneScopedN("GameInitialize");

	ConRegisterVar(&cvar_game);
	ConRegisterVar(&cvar_show_fps);

	ConRegisterCommand("tp", Con_tp, "teleport to a position");
	ConRegisterCommand("map", Con_map, "load a map");
}

void Game::Init() {
	m_entityManager.Init();

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

	if (m_gameMode) m_gameMode->OnTick(dt);
	if (server) server->Tick(dt);
	if (client) client->Tick(dt);
}

void Game::Draw() {
	ZoneScopedN("Game::Draw");

	DrawSetLayer(Layer_Main);

	static float3 test1 = {};
	if (InputGetButton(SCANCODE_X)) test1 = m_activeCamera->position;

	if (level_mesh) {
		DrawMesh(level_mesh, Library::mat_brush, float4x4::Identity());
	}

	m_entityManager.Iterate([](Entity* entity) {
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

void Game::EndGameMode() {
	m_entityManager.Reset();
	m_entityManager.Init();

	if (m_gameMode) {
		m_gameMode->OnEnd();
		m_gameMode->~GameMode();
		Allocator::HeapAllocator.Free(m_gameMode);
	}
}

void Game::SetGameMode(Type* gm_class) {
	EndGameMode();

	m_gameMode = (GameMode*)gm_class->Instantiate(Allocator::HeapAllocator);
	assert(m_gameMode);

	m_gameMode->Init(this, &m_entityManager);
	m_gameMode->OnBegin();
}
