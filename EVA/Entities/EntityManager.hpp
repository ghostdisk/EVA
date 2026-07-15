#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Core/Allocator.hpp>
#include <EVA/Entities/Entity.hpp>

typedef struct _iobuf FILE;

class EntityManager {
public:
	Game*                game                = nullptr;
	Vector<Entity*> entities            = {};
	Vector<Entity*> update_list         = {};
	Vector<Entity*> fixed_update_list   = {};

	template <typename F> // TODO: remove this
	void Iterate(F&& callback) {
		for (Entity* entity : entities) {
			callback(entity);
		}
	}

	Entity* CreateEntity(Type* type, EID eid) {
		Entity* entity = (Entity*)type->Instantiate(Allocator::HeapAllocator);
		RegisterEntity(entity, eid);
		return entity;
	}

	void RegisterEntity(Entity* entity, EID eid) {
		entities.push_back(entity);
		entity->eid = eid;
		entity->OnActivate(EntityCallbackInfo{
			.game = game,
			.entityManager = this,
		});
	}

	void DestroyEntity(Entity* entity) {
		for (int i = 0; i < entities.size(); i++) {
			if (entities[i] == entity) {
				entities[i] = entities.back();
				entities.pop_back();
				break;
			}
		}
		entity->~Entity();
		Allocator::HeapAllocator.Free(entity);
	}

	void Init();
	void Reset();
};

void EntitySetName(Entity* entity, const char* name);
Result EntityLoad(Entity** out_entity, EntityManager* entity_manager, FILE* f);
void EntitySave(FILE* f, Entity* entity, int indent);