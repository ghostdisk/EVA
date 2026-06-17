#include <EVA/Character.hpp>
#include <EVA/Game.hpp>
#include <EVA/Entities.hpp>
#include <EVA/IO.hpp>
#include <EVA/Library.hpp>
#include <SDL3/SDL.h>

template <>
void EntityInit(ECharacter* character)
{
	character->mesh = Library::mesh_character;
}

void CharacterDoMovement(Game* game, ECharacter* entity)
{
	float3 input = {
		(float)IOGetButton(SDL_SCANCODE_D) - (float)IOGetButton(SDL_SCANCODE_A),
		(float)IOGetButton(SDL_SCANCODE_W) - (float)IOGetButton(SDL_SCANCODE_S),
		0,
	};
	if (input.x || input.y) input = input.Normalized();

}