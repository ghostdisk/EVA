#include <EVA/Game.hpp>
#include <EVA/GameModes/BasePlayableGameMode.hpp>
#include <EVA/Assets/Mesh.hpp>
#include <EVA/Entities/Entity.hpp>
#include <EVA/Entities/ECamera.hpp>
#include <EVA/Entities/EntityManager.hpp>
#include <stdio.h>
#include <vector>

void BasePlayableGameMode::OnBegin() {
	m_camera = new ECamera();

	CameraInit(*m_camera);
	m_camera->position.y = -10;
	m_camera->position.z = 3;

	m_entityManager->RegisterEntity(m_camera, EID_DefaultCamera);
	m_game->m_activeCamera = m_camera;
}

void BasePlayableGameMode::OnEnd() {
}

void BasePlayableGameMode::OnTick(double dt) {
	if (g_active_game == m_game) {
		CameraFly(*m_camera);
	}
	CameraUpdateMatrices(*m_camera);
}

Result BasePlayableGameMode::LoadMap(String name) {
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

	Mesh* level_mesh = new Mesh();
	level_mesh->vertices = std::move(vertices);
	level_mesh->indices = std::move(indices);
	level_mesh->Upload(false);

	int num_entities;
	n = fscanf(f, "entities %d\n", &num_entities);
	if (n != 1) return Err("failed to load map");
	assert(num_entities >= 0 && num_entities < 1000);

	for (int i = 0; i < num_entities; i++) {
		Entity* entity = nullptr;
		TRY(EntityLoad(&entity, m_entityManager, f));
	}

	snprintf(map_name, sizeof(map_name), "%.*s", STRING_PRINTF_ARGS(name));
	return Success();
}
