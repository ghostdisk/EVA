#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>

typedef U32 EID;
struct Entity;
struct Mesh;
struct Material;
struct Game;

struct PlayerController {
	float2 input     = {};
	float  input_yaw = {};

	void Tick(double dt);
};

struct EntityTypeMeta {
	char        name[32]             = "";
	float3      editor_box_offset    = float3(0,0,0);
	float3      editor_box_size      = float3(0.5f,0.5f,0.5f);
	Entity*     (*CreateEntity)()    = nullptr;
};

#define X_FOREACH_ENTITY() \
    /* type          type id */ \
	X(StaticMesh,    1) \
	X(Rigidbody,     2) \
	X(Character,     3) \
	X(Marker,        4) \

enum EntityType : U8 {
	EntityType_None       = 0,
	#define X(name, id) EntityType_ ## name = id,
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

struct EntityCallbackInfo {
	Game*  game = nullptr;
	double dt   = 0.0;
};

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

	void RequestUpdateCallback(const EntityCallbackInfo& ci);
	void RequestFixedUpdateCallback(const EntityCallbackInfo& ci);

	virtual void OnActivate(const EntityCallbackInfo& ci) {}
	virtual void OnDeactivate(const EntityCallbackInfo& ci) {}
	virtual void OnUpdate(const EntityCallbackInfo& ci) {}
	virtual void OnFixedUpdate(const EntityCallbackInfo& ci) {}

	virtual ~Entity() {}
};
