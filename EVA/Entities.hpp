#pragma once
#include <EVA/Common.hpp>
#include <EVA/Math.hpp>

typedef U32 EID;
struct Mesh;

enum EntityType : U8
{
	EntityType_None       = 0,
	EntityType_StaticMesh = 1,
};

struct BaseEntity
{
	EID         eid       = 0;
	EntityType  type      = EntityType_None;	
	bool        alive     = false;
	// 2 bytes of waste here

	union
	{
		struct
		{
			float3 position;
			float4 rotation;
			float3 scale;
		};
		BaseEntity* next_free = nullptr;
	};
};

struct EStaticMesh : BaseEntity
{
	Mesh* mesh = nullptr;
};

template <typename TEntity>
struct EntityPool
{
	TEntity* begin     = nullptr;
	TEntity* end       = nullptr;
	TEntity* head      = nullptr;
	TEntity* free_list = nullptr;

	void Init(int capacity)
	{
		begin     = (TEntity*)malloc(capacity);
		end       = begin + capacity;
		head      = begin;
		free_list = nullptr;
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
		entity->eid      = eid;
		entity->alive    = true;
		entity->position = {};
		entity->rotation = {0,0,0,1};
		entity->scale    = {1,1,1};
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
};

inline void EntityManagerInit(EntityManager& entity_manager)
{
	entity_manager.StaticMesh.Init(512);
}