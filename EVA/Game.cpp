#include <EVA/App.hpp>
#include <EVA/Collision.hpp>
#include <EVA/Game.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <EVA/GLTF.hpp>
#include <EVA/Library.hpp>
#include <EVA/Console.hpp>
#include <EVA/CSG.hpp>
#include <EVA/Input.hpp>
#include <EVA/UI.hpp>
#include <EVA/Physics.hpp>
#include <EVA/Physics_Jolt.hpp>
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

	game->physics = PhysicsWorldCreate();

	PhysicsCollider ground_collider = PhysicsCreateBoxCollider({ 128, 0.25, 128 });
	PhysicsCollider box_collider = PhysicsCreateBoxCollider({ 1, 1, 1 });

	PhysicsBody ground = PhysicsCreateBody(game->physics, ground_collider, {0,0,-2}, {0,0,0,1}, true);

	for (int i = 0; i < 10; i++)
	{
		Entity* entity = game->entity_manager.CreateEntity(EntityType_Rigidbody, 1);
		entity->mesh = Library::mesh_cube;
		entity->material = Library::mat_brush;
		entity->position.z = i * 3;
		entity->position.x = i;
		entity->position.y = 0.3 * i;
		PhysicsBody box = PhysicsCreateBody(game->physics, box_collider, entity->position, entity->rotation, false);
		entity->body = box.body;
	}
}

void GameTick(Game* game, double dt)
{
	ZoneScopedN("GameTick");

	PhysicsTick(game->physics, dt);
	// JPH::BodyInterface& body_interface = game->physics->system.GetBodyInterfaceNoLock();

	game->entity_manager.Rigidbody.Iterate(
		[](ERigidbody* rb)
		{
			rb->position = ConvertPos(rb->body->GetPosition());
			rb->rotation = ConvertQuat(rb->body->GetRotation());
		});

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

	static float3 test1 = {};
	if (InputGetButton(SDL_SCANCODE_X)) test1 = g_current_camera->position;

	if (game->level_mesh)
	{
		DrawMesh(game->level_mesh, Library::mat_brush, float4x4::Identity());
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


	int num_vertices;
	n = fscanf(f, "vertices %d", &num_vertices);
	assert(n == 1);

	std::vector<MeshVertex> vertices(num_vertices);
	for (int i = 0; i < num_vertices; i++)
	{
		MeshVertex& vert = vertices[i];
		vert.texcoord = {};
		n = fscanf(f, "%f %f %f %f %f %f", XYZ(&vert.position), XYZ(&vert.normal));
		assert(n == 6);
	}

	int num_indices;
	n = fscanf(f, "\nindices %d", &num_indices);
	assert(n == 1);

	std::vector<U32> indices(num_indices);
	for (int i = 0; i < num_indices; i++)
	{
		n = fscanf(f, "%u", &indices[i]);
		assert(n == 1);
	}

	game->level_mesh = MeshCreate("level_mesh", num_vertices, vertices.data(), num_indices, indices.data());
}