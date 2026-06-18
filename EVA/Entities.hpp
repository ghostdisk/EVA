#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>

typedef U32 EID;
struct Mesh;
struct Material;
struct CharacterCollider;

enum EntityType : U8
{
	EntityType_None       = 0,
	EntityType_StaticMesh = 1,
	EntityType_Rigidbody  = 2,
	EntityType_Character  = 3,

	EntityType_ENUM_SIZE,
};

namespace JPH
{
	class Body;
}
using PhysicsBody = JPH::Body;

// @CONSTRUCTOR_NOT_CALLED
struct Entity
{
	char        name[16];
	EID         eid;
	EntityType  type;
	bool        alive;
	// 2 bytes of waste here

	union
	{
		struct
		{
			float3 position;
			float4 rotation;
			float3 scale;
		};
		Entity* next_free;
	};

	PhysicsBody* body;
	Mesh* mesh;
	Material* material;
};

// @CONSTRUCTOR_NOT_CALLED
struct EStaticMesh : Entity
{
};

// @CONSTRUCTOR_NOT_CALLED
struct ERigidbody : Entity
{
};

struct CharacterController;

// @CONSTRUCTOR_NOT_CALLED
struct ECharacter : Entity
{
	float3 velocity = {};
	CharacterController* controller = nullptr;
	CharacterCollider* collider = nullptr;
};

template <typename T>
inline void EntityInit(T* entity)
{
}

template <>
void EntityInit(ECharacter* character);

template <typename TEntity>
struct EntityPool
{
	TEntity*   begin     = nullptr;
	TEntity*   end       = nullptr;
	TEntity*   head      = nullptr;
	TEntity*   free_list = nullptr;
	EntityType type      = {};

	void Init(int capacity, EntityType type)
	{
		begin     = (TEntity*)malloc(capacity * sizeof(TEntity));
		end       = begin + capacity;
		head      = begin;
		free_list = nullptr;
		this->type = type;
	}

	TEntity* CreateEntity(EID eid)
	{
		TEntity* entity = nullptr;
		if (free_list)
		{
			entity = free_list;
			free_list = (TEntity*)entity->next_free;
		}
		else
		{
			assert(head < end);
			entity = head;
			head++;
		}

		memset(entity, 0, sizeof(TEntity));
		entity->eid      = eid;
		entity->type     = type;
		entity->alive    = true;
		entity->rotation = {0,0,0,1};
		entity->scale    = {1,1,1};
		EntityInit(entity);
		return entity;
	}

	template <typename F>
	void Iterate(F&& callback)
	{
		for (TEntity* ptr = begin; ptr < head; ptr++)
		{
			if (ptr->alive)
			{
				callback(ptr);
			}
		}
	}

	void DestroyEntity(TEntity* entity)
	{
		entity->eid = 0;
		entity->alive = false;
		entity->next_free = free_list;
		free_list = entity;
	}
};

struct EntityManager
{
	EntityPool<EStaticMesh> StaticMesh;
	EntityPool<ERigidbody>  Rigidbody;
	EntityPool<ECharacter>  Character;

	template <typename F>
	void Iterate(F&& callback)
	{
		StaticMesh.Iterate(callback);
		Rigidbody.Iterate(callback);
		Character.Iterate(callback);
	}

	Entity* CreateEntity(EntityType type, EID eid)
	{
		switch (type)
		{
			case EntityType_StaticMesh: return StaticMesh.CreateEntity(eid);
			case EntityType_Rigidbody:  return Rigidbody.CreateEntity(eid);
			case EntityType_Character:  return Character.CreateEntity(eid);
			default: assert(0); return nullptr;
		}
	}
};


void EntityManagerInit(EntityManager& entity_manager);
void EntitySetName(Entity* entity, const char* name);