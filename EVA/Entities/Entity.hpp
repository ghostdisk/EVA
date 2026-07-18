#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Math.hpp>

typedef U32 EID;
class Entity;
class Mesh;
class Material;
class Game;
class EntityManager;

enum EIDs : U32 {
	EID_None = 0,
	EID_DefaultCamera = 1,

	EID_MapStart    = 0x00001000,
	EID_SyncedStart = 0x00100000,
	EID_LocalStart  = 0x80000000,
};

enum EMarkerType {
	EMarkerType_None = 0,
	EMarkerType_PlayerSpawn = 1,
};

enum EntityFlagBits : U32 {
	EntityFlags_None = 0,
};
typedef U32 EntityFlags;

struct EntityCallbackInfo {
	Game*          game           = nullptr;
	EntityManager* entityManager  = nullptr;
	double         dt             = 0.0;
};

class Entity : public Object {
public:
	ECLASS_COMMON(Entity);

	char        name[16]     = "";
	EID         eid          = 0;
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
};
