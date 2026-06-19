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
}

CharacterCollider* CharacterColliderCreate(const char* name, float width, float height, float padding)
{
	CharacterCollider* collider = new CharacterCollider();
	AssetInit(collider, AssetType_CharacterCollider, name);
	collider->width = width;
	collider->height = height;
	collider->padding = padding;
	
	char shape_name[64];
	snprintf(shape_name, 64, "%s_collider", name);
	collider->aabb = PhysicsCreateBoxCollider(float3(width, height, width) / 2);

	return collider;
}

void CharacterAttachController(ECharacter* character, CharacterCollider* collider)
{
	assert(!character->controller);
	character->controller = new CharacterController();
	character->collider = collider;
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
	bool   hit            = false;
	float  fraction       = 0;
	float3 contact_normal = {};
	float3 surface_normal = {};
};

CharacterSweepResult CharacterSweep(Game* game, ECharacter* character, float3 start_position, float3 delta, float4 color)
{
	const JPH::NarrowPhaseQuery&            narrow_phase   = game->physics->system.GetNarrowPhaseQueryNoLock();
	const JPH::BodyLockInterfaceNoLock&    lock_interface = game->physics->system.GetBodyLockInterfaceNoLock();

	struct Hit
	{
		float           fraction      = INFINITY;
		JPH::Vec3       contact_axis  = {};
		JPH::Vec3       point         = {};
		JPH::SubShapeID sub_shape_id  = {};
		JPH::BodyID     body          = {};
		float3          surface_normal; // filled out after the sweep
	};

	struct Collector : JPH::CastShapeCollector
	{
		Game*            game              = nullptr;
		std::vector<Hit> hits              = {};

		virtual void AddHit(const JPH::ShapeCastResult& result) override
		{
			float new_fraction = result.GetEarlyOutFraction();

			Hit hit = {
				.fraction     = new_fraction,
				.contact_axis = result.mPenetrationAxis,
				.point        = result.mContactPointOn2,
				.sub_shape_id = result.mSubShapeID2,
				.body         = result.mBodyID2,
			};
			hits.push_back(hit);
		}
	};

	JPH::Shape* shape = character->collider->aabb->shape;

	float3 center_of_mass_position = start_position;
	center_of_mass_position.z += character->collider->height / 2;
	JPH::RMat44 center_of_mass_start = JPH::RMat44::sTranslation(Convert(center_of_mass_position));

	JPH::RShapeCast shape_cast(shape, JPH::Vec3::sOne(), center_of_mass_start, Convert(delta));

	JPH::ShapeCastSettings  settings;
	settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
	settings.mBackFaceModeConvex = JPH::EBackFaceMode::IgnoreBackFaces;
	settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideOnlyWithActive;

	Collector collector;
	collector.game = game;

	narrow_phase.CastShape(shape_cast, settings, JPH::Vec3(), collector);

	if (collector.hits.size())
	{

		float min_fraction = INFINITY;
		for (Hit& hit : collector.hits)
		{
			if (hit.fraction < min_fraction)
			{
				min_fraction = hit.fraction;
			}
		}


		Hit* best_hit = nullptr;
		float best_dot = -INFINITY;
		for (Hit& hit : collector.hits)
		{
			if (hit.fraction > min_fraction + 0.0001)
			{
				continue;
			}

			JPH::BodyLockRead lock(lock_interface, hit.body);
			if (lock.Succeeded())
			{
				hit.surface_normal = Convert(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.sub_shape_id, hit.point));
				DrawLine(Convert(hit.point), Convert(hit.point) + hit.surface_normal, float4(0, 0, 0, 1));
			}
			else
			{
				continue;
			}

			float dot = Dot(hit.surface_normal, -delta);
			if (dot > best_dot)
			{
				best_hit = &hit;
				best_dot = dot;
			}
		}

		float3 contact_normal = Convert(-best_hit->contact_axis).Normalized();
		float3 surface_normal = contact_normal;
		JPH::BodyLockRead lock(lock_interface, best_hit->body);
		if (lock.Succeeded())
		{
			surface_normal = Convert(lock.GetBody().GetWorldSpaceSurfaceNormal(best_hit->sub_shape_id, best_hit->point));
		}

		float3 hit_point = start_position + delta * best_hit->fraction;
		DrawLine(start_position, hit_point, color);

		return CharacterSweepResult{
			.hit            = true,
			.fraction       = best_hit->fraction,
			.contact_normal = contact_normal,
			.surface_normal = surface_normal,
		};
	}
	else
	{
		DrawLine(start_position, start_position + delta, color);
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


	float3 pos = character->position;

#if 0
	float3 delta = {0, 1, 0};
	glm_quat_rotatev(character->rotation, delta, delta);
	delta *= 5;
#else
	float3 delta = character->velocity * dt;
#endif

	int iter = 0;

	float4 colors[] = {
		{ 1, 1, 1, 1 },
		{ 0, 1, 0, 1 },
		{ 0, 1, 1, 1 },
	};

	for (;;)
	{
		float len = delta.Length();
		if (len < 0.0001f)
		{
			break;
		}
		float3 dir = delta.Normalized();

		iter++;
		if (iter > 3)
		{
			break;
		}

		CharacterSweepResult sweep;
		sweep = CharacterSweep(game, character, pos, delta, colors[iter % EVA_ARRAYSIZE(colors)]);

		if (sweep.hit)
		{

			float4 color = colors[iter % EVA_ARRAYSIZE(colors)];
			float rem_fraction = 1.0f - sweep.fraction;

			float3 normal = sweep.surface_normal;
			normal.z = 0; normal = normal.Normalized();
			if (normal.x == 0 && normal.y == 0)
			{
				break;
			}

			float3 pad = -dir * character->collider->padding * Dot(normal, -dir);
			float3 hit_point = pos + delta * sweep.fraction + pad;

			float3 rem_delta = delta * rem_fraction;
			float3 refl_rem_delta = rem_delta - normal * (2 * Dot(normal, rem_delta));
			float3 proj1 = normal * Dot(refl_rem_delta, normal);
			// float3 p

			DrawLine(pos, hit_point, color);
			DrawAABB(hit_point + float3(0,0,character->collider->height/2), character->collider->Size(), color);

			float3 next_delta = refl_rem_delta - proj1;

			// DrawLine(hit_point, hit_point + sweep.surface_normal, {1,0,0,1});
			// DrawLine(hit_point + float3(0,0,0.04), hit_point + sweep.contact_normal + float3(0,0,0.04), {0,1,0,1});
			delta = next_delta;
			pos = hit_point;
		}
		else
		{
			pos += delta;
			break;
		}
	}

	// character->position += character->velocity * dt;
	character->position = pos;
}
