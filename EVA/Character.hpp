#pragma once

struct ECharacter;
struct Game;
struct CharacterController;

struct CharacterController
{
	int x;
};

void CharacterAttachController(ECharacter* character);
void CharacterDoMovement(Game* game, ECharacter* entity, double dt);