#include <EVA/Game.hpp>
#include <EVA/Entities/Entity.hpp>
#include <EVA/Entities/EntityManager.hpp>
#include <EVA/Entities/ECharacter.hpp>
#include <stdio.h>

void EntityManager::Init() {
}

void EntityManager::Reset() {
	for (Entity* entity : entities) delete entity;

	entities.clear();
	update_list.clear();
	fixed_update_list.clear();
}

void EntitySetName(Entity* entity, const char* name) {
	snprintf(entity->name, sizeof(entity->name), "%s", name);
}

Result EntityLoad(Entity** out_entity, EntityManager* entityManager, FILE* f) {
	int n;
	char buf[32] = {};

	int eid;
	n = fscanf(f, "entity %31s %d\n", buf, &eid);
	if (n != 2) return Err("Failed to parse");

	Type* type = Type::Find(buf);
	if (!type) Err("Type %s doesn't exist", buf);

	Entity* entity = entityManager->CreateEntity(type, eid);

	for (;;) {
		n = fscanf(f, "%s", buf);
		assert(n == 1);

		if (strcmp(buf, "position") == 0) {
			n = fscanf(f, "%f %f %f\n", XYZ(&entity->position));
			assert(n == 3);
		}
		else if (strcmp(buf, "entity_end") == 0) {
			fscanf(f, "\n");
			break;
		}
	}

	entityManager->RegisterEntity(entity, eid);
	*out_entity = entity;
	return Success();
}

static void Indent(FILE* f, int indent) {
	for (int i = 0; i < indent; i++) fprintf(f, "\t");
}

void EntitySave(FILE* f, Entity* entity, int indent) {
	Indent(f, indent); fprintf(f, "entity %s %d\n", entity->GetClass()->name.c_str(), (int)entity->eid);
	indent++;
	Indent(f, indent); fprintf(f, "position %f %f %f\n", XYZ(entity->position));

	indent--;
	Indent(f, indent); fprintf(f, "entity_end\n");
}

void Entity::RequestUpdateCallback(const EntityCallbackInfo& ci) {
	ci.entityManager->update_list.push_back(this);
}

void Entity::RequestFixedUpdateCallback(const EntityCallbackInfo& ci) {
	ci.entityManager->fixed_update_list.push_back(this);
}
