#include <EVA/Entities.hpp>
#include <stdio.h>

void EntityManagerInit(EntityManager& entity_manager)
{
	entity_manager.StaticMesh.Init    (512,   EntityType_StaticMesh);
	entity_manager.Rigidbody.Init     (512,   EntityType_Rigidbody);
	entity_manager.Character.Init     (64,    EntityType_Character);
}

void EntitySetName(Entity* entity, const char* name)
{
	snprintf(entity->name, sizeof(entity->name), "%s", name);
}