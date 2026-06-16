#include <EVA/Physics.hpp>
#include <EVA/Entities.hpp>
#include <Jolt/Jolt.h>
#include <stdio.h>
#include <stdarg.h>
#include <thread>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

static JPH::JobSystemThreadPool JobSystem;

static constexpr JPH::BroadPhaseLayer BroadPhaseLayer_NonMoving(0);
static constexpr JPH::BroadPhaseLayer BroadPhaseLayer_Moving(1);
static constexpr JPH::uint BroadPhaseLayer_NUM_LAYERS = 2;

struct Physics
{
	JPH::PhysicsSystem system;
};

struct ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
	{
		switch (inObject1)
		{
			case PhysicsLayer_NonMoving: return inObject2 == PhysicsLayer_Moving;
			case PhysicsLayer_Moving: return true;
			default: JPH_ASSERT(false); return false;
		}
	}
};

struct BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
	JPH::BroadPhaseLayer object_to_broad_phase[PhysicsLayer_NUM_LAYERS];

	BPLayerInterfaceImpl()
	{
		object_to_broad_phase[PhysicsLayer_NonMoving] = BroadPhaseLayer_NonMoving;
		object_to_broad_phase[PhysicsLayer_Moving] = BroadPhaseLayer_Moving;
	}

	virtual JPH::uint GetNumBroadPhaseLayers() const override
	{
		return BroadPhaseLayer_NUM_LAYERS;
	}

	virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
	{
		JPH_ASSERT(inLayer < PhysicsLayer_NUM_LAYERS);
		return object_to_broad_phase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
	{
		switch ((JPH::BroadPhaseLayer::Type)inLayer)
		{
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayer_NonMoving:	return "NON_MOVING";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayer_Moving:    return "MOVING";
		default: JPH_ASSERT(false); return "INVALID";
		}
	}
#endif
};

struct ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1)
		{
			case PhysicsLayer_NonMoving: return inLayer2 == BroadPhaseLayer_Moving;
			case PhysicsLayer_Moving:    return true;
			default: JPH_ASSERT(false); return false;
		}
	}
};

using TraceFunction = void (*)(const char *inFMT, ...);
void TraceImpl(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}
 

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, JPH::uint inLine)
{
	Fatal("JPH Assertion Failed: %s:%d: %s - %s", inFile, inLine, inExpression, inMessage);
};
#endif

static BPLayerInterfaceImpl              broad_phase_layer_interface;
static ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
static ObjectLayerPairFilterImpl         object_vs_object_layer_filter;

static inline JPH::Vec3 Convert(float3 vec)
{
	return JPH::Vec3(vec.x, vec.z, -vec.y);
}

static inline JPH::Vec3 ConvertSize(float3 vec)
{
	return JPH::Vec3(vec.x, vec.y, vec.z);
}

static inline float3 Convert(JPH::Vec3 vec)
{
	return float3(vec.mF32[0], -vec.mF32[2], vec.mF32[1]);
}

void PhysicsInitialize()
{
	JPH::RegisterDefaultAllocator();
	JPH::Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl);
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	JobSystem.Init(
		JPH::cMaxPhysicsJobs,
		JPH::cMaxPhysicsBarriers, 
		std::thread::hardware_concurrency() - 1);

}

Physics* PhysicsCreate()
{
	Physics* physics = new Physics();
	physics->system.Init(
		PHYSICS_MAX_BODIES, 0, PHYSICS_MAX_BODY_PAIRS, PHYSICS_MAX_CONTACT_CONSTRAINTS,
		broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);
	return physics;
}


void PhysicsTick(Physics* physics, double dt)
{
	JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);
	physics->system.Update(dt, PHYSICS_COLLISION_STEPS, &temp_allocator, &JobSystem);

	{ // sync bodies:
		JPH::BodyIDVector bodies;
		physics->system.GetActiveBodies(JPH::EBodyType::RigidBody, bodies);

		JPH::BodyInterface &body_interface = physics->system.GetBodyInterface();

		for (auto body_id : bodies)
		{
			Entity* entity = (Entity*)body_interface.GetUserData(body_id);
			entity->position = Convert(body_interface.GetPosition(body_id));
		}
	}
}

PhysicsShape* PhysicsCreateBoxShape(float3 size)
{
	JPH::BoxShapeSettings floor_shape_settings(ConvertSize(size));
	floor_shape_settings.SetEmbedded();

	auto shape_ref = floor_shape_settings.Create().Get();
	JPH::Shape* shape = shape_ref.Disown();
	shape->AddRef();
	return shape;
}

void PhysicsAttachBodyToEntity(Physics* physics, Entity* entity, PhysicsShape* shape, PhysicsLayer layer)
{
	JPH::BodyInterface &body_interface = physics->system.GetBodyInterface();

	JPH::EMotionType motion_type = JPH::EMotionType::Dynamic;
	if (layer == PhysicsLayer_NonMoving)
	{
		motion_type = JPH::EMotionType::Static;
	}

	JPH::BodyCreationSettings body_creation_settings(
		shape, Convert(entity->position), {0,0,0,1}, motion_type, layer);

	JPH::Body* body = body_interface.CreateBody(body_creation_settings);
	body_interface.AddBody(body->GetID(), JPH::EActivation::Activate);

	body->SetUserData((U64)entity);
	entity->body = body;
}