#pragma once
#include <EVA/Math.hpp>

struct PhysicsWorld;

namespace JPH
{
	class Shape;
}

struct PhysicsTriangle
{
	float3 points[3];
};

struct PhysicsCollider
{
	JPH::Shape* shape = nullptr;
};

void PhysicsInitialize();
PhysicsWorld* PhysicsWorldCreate();
void PhysicsWorldDestroy(PhysicsWorld* world);
void PhysicsTick(PhysicsWorld* world, double dt);

PhysicsCollider PhysicsCreateBoxCollider(const float3& half_extents);
PhysicsCollider PhysicsCreateMeshCollider(size_t num_triangles, PhysicsTriangle* triangles);
void PhysicsDestroyCollider(PhysicsCollider& collider);