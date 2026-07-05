#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <stdio.h> // FILE

typedef U32 EID;
struct Mesh;
struct Material;

namespace JPH {
	class Body;
}

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

// @CONSTRUCTOR_NOT_CALLED
struct Entity {
	char        name[16];
	EID         eid;
	EntityType  type;
	bool        alive;
	// 2 bytes of waste here

	union {
		struct {
			float3 position;
			float4 rotation;
			float3 scale;
		};
		Entity* next_free;
	};

	Mesh* mesh;
	Material* material;
	JPH::Body* body;
};

// @CONSTRUCTOR_NOT_CALLED
struct EStaticMesh : Entity {
};

// @CONSTRUCTOR_NOT_CALLED
struct ERigidbody : Entity {
};

// @CONSTRUCTOR_NOT_CALLED
struct EMarker : Entity {
	EMarkerType marker_type = {};
	int         team_index  = 0;
};


// @CONSTRUCTOR_NOT_CALLED
struct ECharacter : Entity {
	float3 velocity = {};
};

template <typename T>
inline void EntityInit(T* entity) {
}

template <typename TEntity>
struct EntityPool {
	TEntity*   begin     = nullptr;
	TEntity*   end       = nullptr;
	TEntity*   head      = nullptr;
	TEntity*   free_list = nullptr;
	EntityType type      = {};

	void Init(int capacity, EntityType type) {
		begin     = (TEntity*)malloc(capacity * sizeof(TEntity));
		end       = begin + capacity;
		head      = begin;
		free_list = nullptr;
		this->type = type;
	}

	void Deinit() {
		free(begin);
		*this = {};
	}

	TEntity* CreateEntity(EID eid) {
		TEntity* entity = nullptr;
		if (free_list) {
			entity = free_list;
			free_list = (TEntity*)entity->next_free;
		} else {
			assert(head < end);
			entity = head;
			head++;
		}

		memset(entity, 0, sizeof(TEntity));
		entity->eid      = eid;
		entity->type     = type;
		entity->alive    = true;
		entity->rotation = {0,0,0,1};
		entity->scale    = {1,1,1};
		EntityInit(entity);
		return entity;
	}

	template <typename F>
	void Iterate(F&& callback) {
		for (TEntity* ptr = begin; ptr < head; ptr++) {
			if (ptr->alive) {
				callback(ptr);
			}
		}
	}

	void DestroyEntity(TEntity* entity) {
		entity->eid = 0;
		entity->alive = false;
		entity->next_free = free_list;
		free_list = entity;
	}
};

struct EntityManager {

#define X(name, id, lim) EntityPool<E ## name> pool_ ## name;
X_FOREACH_ENTITY()
#undef X

	template <typename F>
	void Iterate(F&& callback) {
		#define X(name, id, lim) pool_ ## name.Iterate(callback);
		X_FOREACH_ENTITY()
		#undef X
	}

	Entity* CreateEntity(EntityType type, EID eid) {
		switch (type) {
			#define X(name, id, lim) case EntityType_ ## name: return pool_ ## name.CreateEntity(eid);
			X_FOREACH_ENTITY()
			#undef X
			default: assert(0); return nullptr;
		}
	}

	void DestroyEntity(Entity* entity) {
		switch (entity->type) {
			#define X(name, id, lim) case EntityType_ ## name: pool_ ## name.DestroyEntity((E ## name*)entity); return;
			X_FOREACH_ENTITY()
			#undef X
			default: assert(0); return;
		}
	}
};

void EntityManagerInit(EntityManager& entity_manager);
void EntityManagerDeinit(EntityManager& entity_manager);
void EntitySetName(Entity* entity, const char* name);
Entity* EntityLoad(EntityManager* entity_manager, FILE* f);
void EntitySave(FILE* f, Entity* entity, int indent);