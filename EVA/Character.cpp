#include <EVA/Character.hpp>
#include <EVA/Game.hpp>
#include <EVA/Entities.hpp>
#include <EVA/IO.hpp>
#include <EVA/Library.hpp>
#include <EVA/PhysicsInternal.hpp>
#include <EVA/Renderer.hpp>
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
	}

	float angles[3] = { 0, 0, game->camera.yaw };
	glm_euler_xyz_quat(angles, character->rotation);
	glm_quat_rotatev(character->rotation, input, input);

	float speed = 3;
	if (IOGetButton(SDL_SCANCODE_LSHIFT)) speed = 10;
	if (IOGetButton(SDL_SCANCODE_LCTRL)) speed = 0.5;

	character->velocity = input * speed;
}

struct CharacterSweepResult
{
	bool   hit       = false;
	float  fraction  = 0;
	float  distance  = 0;
	float3 normal    = {};
};

CharacterSweepResult CharacterSweep(Game* game, ECharacter* character, float3 start_position, float3 delta, float4 color)
{
	const JPH::NarrowPhaseQuery& narrow_phase = game->physics->system.GetNarrowPhaseQuery();

	struct Collector : JPH::CastShapeCollector
	{
		Game* game;
		float fraction = INFINITY;
		JPH::Vec3 normal;
		virtual void AddHit(const JPH::ShapeCastResult& result) override
		{
			Entity* colliding_entity = (Entity*)game->physics->system.GetBodyInterfaceNoLock().GetUserData(result.mBodyID2);

			float new_fraction = result.GetEarlyOutFraction();


			if (new_fraction < this->fraction)
			{
				this->fraction = new_fraction;
				normal = result.mPenetrationAxis;
			}
		}
	};

	JPH::Shape* shape = Library::collider_character->shape;

	float3 center_of_mass_position = start_position;
	center_of_mass_position.z += character->height / 2 + 0.01;
	JPH::RMat44 center_of_mass_start = JPH::RMat44::sTranslation(Convert(center_of_mass_position));

	JPH::RShapeCast shape_cast(shape, JPH::Vec3::sOne(), center_of_mass_start, Convert(delta));

	JPH::ShapeCastSettings  settings;
	settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
	settings.mBackFaceModeConvex = JPH::EBackFaceMode::IgnoreBackFaces;
	settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideOnlyWithActive;

	Collector collector;
	collector.game = game;

	narrow_phase.CastShape(shape_cast, settings, JPH::Vec3(), collector);

	if (collector.fraction < INFINITY)
	{
		float3 normal = Convert(-collector.normal).Normalized();

		float3 hit_point = center_of_mass_position + delta * collector.fraction;
		DrawAABB(hit_point, float3(0.25, 0.25, 1.8), color);
		DrawLine(center_of_mass_position, hit_point, color);

		return CharacterSweepResult{
			.hit       = true,
			.fraction  = collector.fraction,
			.distance  = collector.fraction * delta.Length(),
			.normal    = normal,
		};
	}
	else
	{
		DrawLine(center_of_mass_position, center_of_mass_position + delta, color);
		return {
			.hit = false,
		};
	}
}

void CharacterDoMovement(Game* game, ECharacter* character, double dt)
{
	if (character->controller)
	{
		CharacterControllerTick(game, character);
	}


	float3  pos = character->position;
	float3 dir = {0, 1, 0};
	glm_quat_rotatev(character->rotation, dir, dir);
	dir *= 5;

	int iter = 0;

	float4 colors[] = {
		{ 1, 0, 0, 1 },
		{ 0, 1, 0, 1 },
		{ 0, 1, 1, 1 },
	};

	for (;;)
	{
		iter++;
		if (iter > 3)
		{
			printf("Sanity exit\n");
			break;
		}

		CharacterSweepResult sweep;
		sweep = CharacterSweep(game, character, pos, dir, colors[iter % EVA_ARRAYSIZE(colors)]);

		if (sweep.hit)
		{
			float3 hit_point = pos + dir * (sweep.fraction - 0.001f);

			dir = dir * (1.0f - sweep.fraction);
			float3 refl = dir - sweep.normal * (2 * Dot(sweep.normal, dir));

			float3 proj = sweep.normal * Dot(refl, sweep.normal);

			dir = dir + proj;
			pos = hit_point;
			// DrawLine(hit_point, hit_point + dir, {0,1,0,1});
		}
		else
		{
			break;
		}
	}


	character->position += character->velocity * dt;
}
