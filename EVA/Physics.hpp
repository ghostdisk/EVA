#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <EVA/Asset.hpp>

#define PHYSICS_MAX_BODIES              1024
#define PHYSICS_MAX_BODY_PAIRS          1024
#define PHYSICS_MAX_CONTACT_CONSTRAINTS 1024
#define PHYSICS_COLLISION_STEPS         1

struct Physics;
struct Entity;
struct Mesh;
struct MeshVertex;

enum PhysicsLayer : U16
{
	PhysicsLayer_NonMoving = 0,
	PhysicsLayer_Moving = 1,

	PhysicsLayer_NUM_LAYERS,
};

void PhysicsInitialize();

Physics*      PhysicsCreate();
void          PhysicsTick(Physics* physics, double dt);

struct Collider
{
	int x;
};

Collider* PhysicsCreateBoxCollider(float3 size);
Collider* PhysicsCreateMeshCollider( const char* name, size_t num_vertices, const MeshVertex* vertices, size_t num_indices, const U32* indices);
void      PhysicsAttachStaticCollider(Physics* physics, Entity* entity, Collider* shape, PhysicsLayer layer);

