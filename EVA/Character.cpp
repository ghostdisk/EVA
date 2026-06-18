#include <EVA/Character.hpp>
#include <EVA/Game.hpp>
#include <EVA/Entities.hpp>
#include <EVA/IO.hpp>
#include <EVA/Library.hpp>
#include <EVA/PhysicsInternal.hpp>
#include <SDL3/SDL.h>
#include <cglm/euler.h>
#include <cglm/quat.h>
#include <stdio.h>


template <>
void EntityInit(ECharacter* character)
{
	character->mesh = Library::mesh_character;
	character->height = 1.8f;
}

void CharacterAttachController(ECharacter* character)
{
	assert(!character->controller);
	character->controller = new CharacterController();
}

void CharacterControllerTick(Game* game, ECharacter* character)
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
		glm_euler_xyz_quat(angles, character->rotation);
		glm_quat_rotatev(character->rotation, input, input);

		character->velocity = input * 10;
	}
	else
	{
		character->velocity = {};
	}
}

void CharacterDoMovement(Game* game, ECharacter* character, double dt)
{
	if (character->controller)
	{
		CharacterControllerTick(game, character);
	}

	const JPH::NarrowPhaseQuery& narrow_phase = game->physics->system.GetNarrowPhaseQuery();

	printf("--------\n");
	struct Collector : JPH::CastShapeCollector
	{
		Game* game;
		virtual void AddHit(const JPH::ShapeCastResult &result) override
		{
			Entity* colliding_entity = (Entity*)game->physics->system.GetBodyInterfaceNoLock().GetUserData(result.mBodyID2);
			printf("Colliding with %s\n", colliding_entity->name);
		}
	};

	JPH::Shape* shape = Library::collider_character->shape;
	JPH::Vec3 dir = Convert(character->velocity * dt);

	float3 center_of_mass_position = character->position;
	center_of_mass_position.z += character->height / 2 - 0.01;
	JPH::RMat44 center_of_mass_start = JPH::RMat44::sTranslation(Convert(center_of_mass_position));

	JPH::RShapeCast shape_cast(shape, JPH::Vec3::sOne(), center_of_mass_start, dir);

	JPH::ShapeCastSettings  settings;
	settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
	settings.mBackFaceModeConvex = JPH::EBackFaceMode::IgnoreBackFaces;
	settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideOnlyWithActive;

	Collector collector;
	collector.game = game;


	narrow_phase.CastShape(shape_cast, settings, JPH::Vec3(), collector);

	character->position += character->velocity * dt;
}
