#include <EVA/Character.hpp>
#include <EVA/Game.hpp>
#include <EVA/Entities.hpp>
#include <EVA/IO.hpp>
#include <EVA/Library.hpp>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <cglm/euler.h>
#include <cglm/quat.h>

template <>
void EntityInit(ECharacter* character)
{
	character->mesh = Library::mesh_character;
}

void CharacterDoMovement(Game* game, ECharacter* entity, double dt)
{
	float3 input = {
		(float)IOGetButton(SDL_SCANCODE_D) - (float)IOGetButton(SDL_SCANCODE_A),
		(float)IOGetButton(SDL_SCANCODE_W) - (float)IOGetButton(SDL_SCANCODE_S),
		0,
	};
	if (input.x || input.y)
	{
		input = input.Normalized();

		float3 camera_forward = {
			-sinf(game->camera.yaw),
			cosf(game->camera.yaw),
			0,
		};
		float angles[3] = { 0, 0, game->camera.yaw };
		glm_euler_xyz_quat(angles, entity->rotation);
		glm_quat_rotatev(entity->rotation, input, input);

		entity->position += input * dt * 10;
	}

}