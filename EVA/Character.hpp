#pragma once
#include <EVA/Asset.hpp>
#include <EVA/Math.hpp>

struct ECharacter;
struct Game;
struct Collider;
struct CharacterController;

struct CharacterController
{
	int x;
};

struct CharacterCollider : Asset
{
	Collider* capsule;
	Collider* aabb;
	Collider* padded_aabb;
	float     width;
	float     padding;
	float     height;

	float3 Size() { return float3(width, width, height); }
	float3 PaddedSize() { return float3(width + padding, width + padding, height); }
};

void               CharacterAttachController(ECharacter* character, CharacterCollider* collider);
void               CharacterDoMovement(Game* game, ECharacter* entity, double dt);
CharacterCollider* CharacterColliderCreate(const char* name, float width, float height, float padding);