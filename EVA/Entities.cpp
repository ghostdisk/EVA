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
	.editor_box_offset = {0,0,1},
	.editor_box_size = {0.5, 0.5, 2},
};

EntityTypeMeta EMarkerMeta = {
	.name = "EMarker",
	.editor_box_offset = {0,0,.25},
	.editor_box_size = {.25, .25, .25},
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
	#define X(name, id, lim) entity_manager.pool_ ## name.Init(lim,   EntityType_ ## name);
	X_FOREACH_ENTITY()
	#undef X
}

void EntityManagerDeinit(EntityManager& entity_manager)
{
	#define X(name, id, lim) entity_manager.pool_ ## name.Deinit();
	X_FOREACH_ENTITY()
	#undef X
}

void EntitySetName(Entity* entity, const char* name)
{
	snprintf(entity->name, sizeof(entity->name), "%s", name);
}

Entity* EntityLoad(EntityManager* entity_manager, FILE* f)
{
	int n;
	char buf[32] = {};

	int eid, type;
	n = fscanf(f, "entity %d %d\n", &type, &eid);
	assert(n == 2);

	Entity* entity = entity_manager->CreateEntity((EntityType)type, eid);

	for (;;)
	{
		n = fscanf(f, "%s", buf);
		assert(n == 1);

		if (strcmp(buf, "type") == 0)
		{
			int type;
			n = fscanf(f, "%d\n", &type);
			assert(n == 1);
			assert(type > EntityType_None && type < EntityType_ENUM_SIZE);
			entity->type = (EntityType)type;
		}
		else if (strcmp(buf, "position") == 0)
		{
			n = fscanf(f, "%f %f %f\n", XYZ(&entity->position));
			assert(n == 3);
		}
		else if (strcmp(buf, "entity_end") == 0)
		{
			fscanf(f, "\n");
			break;
		}
	}

	return entity;
}

static void Indent(FILE* f, int indent)
{
	for (int i = 0; i < indent; i++) fprintf(f, "\t");
}

void EntitySave(FILE* f, Entity* entity, int indent)
{
	Indent(f, indent); fprintf(f, "entity %d %d\n", (int)entity->type, (int)entity->eid);
	indent++;
	Indent(f, indent); fprintf(f, "position %f %f %f\n", XYZ(entity->position));

	indent--;
	Indent(f, indent); fprintf(f, "entity_end\n");
}
