#pragma once
#include <EVA/Asset.hpp>
#include <EVA/Math.hpp>

struct ECharacter;
struct Game;
struct Collider;

void CharacterDoMovement(Game* game, ECharacter* entity, double dt);