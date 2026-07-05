#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <stdio.h> // FILE
#include <vector>

typedef U32 EID;
struct Mesh;
struct Material;

struct PlayerController {
	float2 input     = {};
	float  input_yaw = {};

	void Tick(double dt);
};

struct EntityTypeMeta {
	char        name[32]          = "";
	float3      editor_box_offset = float3(0,0,0);
	float3      editor_box_size   = float3(0.5f,0.5f,0.5f);
};

#define X_FOREACH_ENTITY() \
    /* type          type id        limit*/ \
	X(StaticMesh,    1,             512) \
	X(Rigidbody,     2,             512) \
	X(Character,     3,             512) \
	X(Marker,        4,             512) \

enum EntityType : U8 {
	EntityType_None       = 0,
	#define X(name, id, lim) EntityType_ ## name = id,
	X_FOREACH_ENTITY()
	#undef X
	EntityType_ENUM_SIZE,
};
extern EntityTypeMeta* ENTITY_TYPE_META[EntityType_ENUM_SIZE];

enum EMarkerType {
	EMarkerType_None = 0,
	EMarkerType_PlayerSpawn = 1,
};

enum EntityFlagBits : U32 {
	EntityFlags_None = 0,
};
typedef U32 EntityFlags;

struct Entity {
	char        name[16]     = "";
	EID         eid          = 0;
	EntityType  type         = EntityType_None;
	U32         entity_flags = EntityFlags_None;
	float3      position     = {0,0,0};
	float4      rotation     = {0,0,0,1};
	float3      scale        = {1,1,1};
	Mesh*       mesh         = nullptr;
	Material*   material     = nullptr;

	void RequestUpdateCallback();
	void RequestFixedUpdateCallback();

	virtual void OnActivate() {}
	virtual void OnDeactivate() {}
	virtual void OnUpdate() {}
	virtual void OnFixedUpdate() {}
};

struct EStaticMesh : Entity {
};

struct ERigidbody : Entity {
};

// @CONSTRUCTOR_NOT_CALLED
struct EMarker : Entity {
	EMarkerType marker_type = {};
	int         team_index  = 0;
};

// @CONSTRUCTOR_NOT_CALLED
struct ECharacter : Entity {
	PlayerController controller;

	void Tick(double dt);
};

template <typename T>
inline void EntityInit(T* entity) {
}

struct EntityManager {
	std::vector<Entity*> entities = {};
	std::vector<Entity*> update_list = {};
	std::vector<Entity*> fixed_update_list = {};

	template <typename F> // TODO: remove this
	void Iterate(F&& callback) {
		for (Entity* entity : entities) {
			callback(entity);
		}
	}

	Entity* CreateEntity(EntityType type, EID eid) {
		Entity* entity = nullptr;
		switch (type) {
			#define X(name, id, lim) case EntityType_ ## name: entity = new E ## name(); break;
			X_FOREACH_ENTITY()
			#undef X
			default: assert(0); return nullptr;
		}
		entities.push_back(entity);
		return entity;
	}

	void DestroyEntity(Entity* entity) {
		for (int i = 0; i < entities.size(); i++) {
			if (entities[i] == entity) {
				entities[i] = entities.back();
				entities.pop_back();
				break;
			}
		}
		delete entity;
	}
};

void EntityManagerInit(EntityManager& entity_manager);
void EntityManagerDeinit(EntityManager& entity_manager);
void EntitySetName(Entity* entity, const char* name);
Entity* EntityLoad(EntityManager* entity_manager, FILE* f);
void EntitySave(FILE* f, Entity* entity, int indent);