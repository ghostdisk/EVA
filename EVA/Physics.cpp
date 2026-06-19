#include <EVA/Physics.hpp>

struct Physics
{
	int x;
};

void PhysicsInitialize()
{
}

Physics* PhysicsCreate()
{
	return new Physics();
}

void PhysicsTick(Physics* physics, double dt)
{
}

Collider* PhysicsCreateBoxCollider(float3 size)
{
	return new Collider();
}

Collider* PhysicsCreateMeshCollider( const char* name, size_t num_vertices, const MeshVertex* vertices, size_t num_indices, const U32* indices)
{
	return new Collider();
}

void      PhysicsAttachStaticCollider(Physics* physics, Entity* entity, Collider* shape, PhysicsLayer layer)
{

}
