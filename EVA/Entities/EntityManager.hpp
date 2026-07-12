#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Entities/Entity.hpp>
#include <vector>

struct EntityManager {
	Game*                game                = nullptr;
	std::vector<Entity*> entities            = {};
	std::vector<Entity*> update_list         = {};
	std::vector<Entity*> fixed_update_list   = {};

	template <typename F> // TODO: remove this
	void Iterate(F&& callback) {
		for (Entity* entity : entities) {
			callback(entity);
		}
	}

	void RegisterEntity(Entity* entity, EID eid) {
		entities.push_back(entity);
		entity->OnActivate(EntityCallbackInfo{
			.game = game,
			.entity_manager = this,
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
		delete entity;
	}
};

void EntityManagerInit(EntityManager& entity_manager);
void EntityManagerDeinit(EntityManager& entity_manager);
void EntitySetName(Entity* entity, const char* name);
Entity* EntityLoad(EntityManager* entity_manager, FILE* f);
void EntitySave(FILE* f, Entity* entity, int indent);