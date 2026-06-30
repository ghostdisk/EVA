#pragma once
#include <EVA/Math.hpp>

struct MeshVertex;
struct PhysicsWorld;

namespace JPH
{
	class Shape;
	class Body;
}

struct PhysicsCollider
{
	JPH::Shape* shape = nullptr;
};

struct PhysicsBody
{
	JPH::Body* body = nullptr;
};

void PhysicsInitialize();
PhysicsWorld* PhysicsWorldCreate();
void PhysicsWorldDestroy(PhysicsWorld* world);
void PhysicsTick(PhysicsWorld* world, double dt);

PhysicsCollider PhysicsCreateBoxCollider(const float3& half_extents);
PhysicsCollider PhysicsCreateMeshCollider(size_t num_vertices, MeshVertex* vertices, size_t num_indices, U32* indices);
void            PhysicsDestroyCollider(PhysicsCollider& collider);

PhysicsBody PhysicsCreateBody(PhysicsWorld* world, PhysicsCollider collider, const float3& position, const float4& rotation, bool is_static);
void        PhysicsDestroyBody(PhysicsWorld* world, PhysicsBody body);