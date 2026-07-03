#include <EVA/Entities.hpp>
#include <stdio.h>

EntityTypeMeta ENoneMeta = {
	.name = "<None>",
};

EntityTypeMeta EStaticMeshMeta = {
	.name = "EStaticMesh",
};

EntityTypeMeta ERigidbodyMeta = {
	.name = "ERigidbody",
};

EntityTypeMeta ECharacterMeta = {
	.name = "ECharacter",
	.editor_box_offset = {0,1,0},
	.editor_box_size = {0.5, 2, 0.5},
};

EntityTypeMeta EMarkerMeta = {
	.name = "EMarker",
	.editor_box_offset = {0,.5,0},
	.editor_box_size = {.5, .5, .5},
};

EntityTypeMeta* ENTITY_TYPE_META[EntityType_ENUM_SIZE]
{
	&ENoneMeta,
	&EStaticMeshMeta,
	&ERigidbodyMeta,
	&ECharacterMeta,
	&EMarkerMeta,
};

void EntityManagerInit(EntityManager& entity_manager)
{

	#define X(name, id, lim) entity_manager.pool_ ## name.Init    (lim,   EntityType_ ## name);
	X_FOREACH_ENTITY()
	#undef X
}

void EntitySetName(Entity* entity, const char* name)
{
	snprintf(entity->name, sizeof(entity->name), "%s", name);
}