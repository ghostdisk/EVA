#include <EVA/Game.hpp>
#include <EVA/Entities/Entity.hpp>
#include <EVA/Entities/EntityManager.hpp>
#include <EVA/Entities/ECharacter.hpp>
#include <stdio.h>

EntityTypeMeta ENoneMeta = {
	.name = "<None>",
};

EntityTypeMeta EStaticMeshMeta = {
	.name = "EStaticMesh",
	.CreateEntity = []() { return new Entity(); }
};

EntityTypeMeta ERigidbodyMeta = {
	.name = "ERigidbody",
};

EntityTypeMeta ECharacterMeta = {
	.name = "ECharacter",
	.editor_box_offset = {0,0,1},
	.editor_box_size = {0.5, 0.5, 2},
	.CreateEntity = []() -> Entity* { return new ECharacter(); }
};

EntityTypeMeta EMarkerMeta = {
	.name = "EMarker",
	.editor_box_offset = {0,0,.25},
	.editor_box_size = {.25, .25, .25},
	.CreateEntity = []() -> Entity* { return new Entity(); }
};

EntityTypeMeta* ENTITY_TYPE_META[EntityType_ENUM_SIZE] {
	&ENoneMeta,
	#define X(name, id, lim) &E ## name ## Meta,
	X_FOREACH_ENTITY()
	#undef X
};

void EntityManagerInit(EntityManager& entity_manager) {
}

void EntityManagerDeinit(EntityManager& entity_manager) {
	for (Entity* entity : entity_manager.entities) delete entity;
	entity_manager.entities.clear();
	entity_manager.update_list.clear();
	entity_manager.fixed_update_list.clear();
}

void EntitySetName(Entity* entity, const char* name) {
	snprintf(entity->name, sizeof(entity->name), "%s", name);
}

Entity* EntityLoad(EntityManager* entity_manager, FILE* f) {
	int n;
	char buf[32] = {};

	int eid, type;
	n = fscanf(f, "entity %d %d\n", &type, &eid);
	assert(n == 2);

	Entity* entity = ENTITY_TYPE_META[type]->CreateEntity();
	entity->type = (EntityType)type;
	entity_manager->RegisterEntity(entity, eid);

	for (;;) {
		n = fscanf(f, "%s", buf);
		assert(n == 1);

		if (strcmp(buf, "type") == 0) {
			int type;
			n = fscanf(f, "%d\n", &type);
			assert(n == 1);
			assert(type > EntityType_None && type < EntityType_ENUM_SIZE);
			entity->type = (EntityType)type;
		}
		else if (strcmp(buf, "position") == 0) {
			n = fscanf(f, "%f %f %f\n", XYZ(&entity->position));
			assert(n == 3);
		}
		else if (strcmp(buf, "entity_end") == 0) {
			fscanf(f, "\n");
			break;
		}
	}

	return entity;
}

static void Indent(FILE* f, int indent) {
	for (int i = 0; i < indent; i++) fprintf(f, "\t");
}

void EntitySave(FILE* f, Entity* entity, int indent) {
	Indent(f, indent); fprintf(f, "entity %d %d\n", (int)entity->type, (int)entity->eid);
	indent++;
	Indent(f, indent); fprintf(f, "position %f %f %f\n", XYZ(entity->position));

	indent--;
	Indent(f, indent); fprintf(f, "entity_end\n");
}

void Entity::RequestUpdateCallback(const EntityCallbackInfo& ci) {
	ci.game->entity_manager.update_list.push_back(this);
}

void Entity::RequestFixedUpdateCallback(const EntityCallbackInfo& ci) {
	ci.game->entity_manager.fixed_update_list.push_back(this);
}
