#include <EVA/Character.hpp>
#include <EVA/Game.hpp>
#include <EVA/Entities.hpp>
#include <EVA/Input.hpp>
#include <EVA/Console.hpp>
#include <EVA/Library.hpp>
#include <EVA/Renderer/Renderer.hpp>
#include <cglm/euler.h>
#include <cglm/quat.h>

template <>
void EntityInit(ECharacter* character) {
}

void CharacterControllerTick(Game* game, ECharacter* character) {
	float3 input = {
		cvar_right.fvalue - cvar_left.fvalue,
		cvar_forward.fvalue - cvar_back.fvalue,
		0,
	};
	if (input.x || input.y)
	{
		input = input.Normalized();
	}

	float angles[3] = { 0, 0, game->camera.yaw };
	glm_euler_xyz_quat(angles, character->rotation);
	glm_quat_rotatev(character->rotation, input, input);

	float speed = 3;
	if (InputGetButton(SDL_SCANCODE_LSHIFT)) speed = 10;
	if (InputGetButton(SDL_SCANCODE_LCTRL)) speed = 0.5;

	character->velocity = input * speed;
}

void CharacterDoMovement(Game* game, ECharacter* character, double dt) {
	CharacterControllerTick(game, character);

	float3 pos = character->position;

#if 0
	float3 delta = {0, 1, 0};
	glm_quat_rotatev(character->rotation, delta, delta);
	delta *= 5;
#else
	float3 delta = character->velocity * dt;
#endif

	pos += delta;

	// pos = CharacterUnstuck(game, character, pos, dt);
	character->position = pos;
}
