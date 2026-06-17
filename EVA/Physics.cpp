#include <EVA/Physics.hpp>
#include <EVA/Entities.hpp>
#include <EVA/Renderer.hpp>
#include <stdio.h>
#include <stdarg.h>
#include <thread>

#include <Jolt/Jolt.h>
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
#include <Jolt/Renderer/DebugRenderer.h>
#include <tracy/Tracy.hpp>

static JPH::JobSystemThreadPool JobSystem;

static constexpr JPH::BroadPhaseLayer BroadPhaseLayer_NonMoving(0);
static constexpr JPH::BroadPhaseLayer BroadPhaseLayer_Moving(1);
static constexpr JPH::uint BroadPhaseLayer_NUM_LAYERS = 2;

struct Physics
{
	JPH::PhysicsSystem  system;
	JPH::TempAllocatorImpl* temp_allocator;
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

struct DebugRendererImpl : JPH::DebugRenderer
{

	struct MeshWrapper : JPH::RefTargetVirtual
	{
		Mesh* mesh = nullptr;
		virtual void AddRef() {}
		virtual void Release() {}
	};

	virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
	{
		printf("Stub: DrawLine");
	}
	virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off)
	{
		printf("Stub: DrawTriangle\n");
	}
	virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view &inString, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f)
	{
		printf("Stub: DrawText3D\n");
	}
	virtual Batch CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount)
	{
		printf("Stub: CreateTriangleBatch\n");
		return {};
	}

	virtual Batch CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, const JPH::uint32 *inIndices, int inIndexCount)
	{
		static int k = 0;
		char name[64];
		snprintf(name, 64, "JoltTriangleBatch_%d", k++);

		std::vector<MeshVertex> vertices(inVertexCount);
		for (int i = 0; i < inVertexCount; i++)
		{
			vertices[i].position.x = inVertices[i].mPosition.x;
			vertices[i].position.y = inVertices[i].mPosition.y;
			vertices[i].position.z = inVertices[i].mPosition.z;
			vertices[i].normal.x   = inVertices[i].mNormal.x;
			vertices[i].normal.y   = inVertices[i].mNormal.y;
			vertices[i].normal.z   = inVertices[i].mNormal.z;
			vertices[i].texcoord.x = inVertices[i].mUV.x;
			vertices[i].texcoord.y = inVertices[i].mUV.y;
		}

		Mesh* mesh = MeshCreate(name, vertices.size(), vertices.data(), inIndexCount, inIndices);

		MeshWrapper* wrapper = new MeshWrapper();
		wrapper->mesh = mesh;
		return wrapper;
	}

	virtual void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox &inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
	{
		Batch batch = inGeometry->mLODs[0].mTriangleBatch;
		MeshWrapper* mesh_wrapper = (MeshWrapper*)batch.GetPtr();

		float4x4 matrix = {};
		memcpy(&matrix, &inModelMatrix, 64);

		float tmp;

		tmp = matrix.data[0][1];
		matrix.data[0][1] = -matrix.data[0][2];
		matrix.data[0][2] = tmp;

		tmp = matrix.data[1][1];
		matrix.data[1][1] = -matrix.data[1][2];
		matrix.data[1][2] = tmp;

		tmp = matrix.data[2][1];
		matrix.data[2][1] = -matrix.data[2][2];
		matrix.data[2][2] = tmp;

		tmp = matrix.data[3][1];
		matrix.data[3][1] = -matrix.data[3][2];
		matrix.data[3][2] = tmp;

		DrawMesh(mesh_wrapper->mesh, matrix);
	}

	void Init()
	{
		Initialize();
	}
};

static BPLayerInterfaceImpl              broad_phase_layer_interface;
static ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
static ObjectLayerPairFilterImpl         object_vs_object_layer_filter;
static DebugRendererImpl                 debug_renderer;

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

static inline float4 ConvertQuat(JPH::Quat quat)
{
	return float4(
		 quat.mValue[0],
		-quat.mValue[2],
		 quat.mValue[1],
		 quat.mValue[3]);
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

	debug_renderer.Init();
}

Physics* PhysicsCreate()
{
	Physics* physics = new Physics();
	physics->temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

	physics->system.Init(
		PHYSICS_MAX_BODIES, 0, PHYSICS_MAX_BODY_PAIRS, PHYSICS_MAX_CONTACT_CONSTRAINTS,
		broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);

	return physics;
}

void PhysicsTick(Physics* physics, double dt)
{
	ZoneScopedN("PhysicsTick");
	{
		ZoneScopedN("Physics Update");
		physics->system.Update(dt, PHYSICS_COLLISION_STEPS, physics->temp_allocator, &JobSystem);
	}

	{ // sync bodies:
		ZoneScopedN("Physics Sync");

		JPH::BodyIDVector bodies;
		physics->system.GetActiveBodies(JPH::EBodyType::RigidBody, bodies);

		JPH::BodyInterface &body_interface = physics->system.GetBodyInterfaceNoLock();

		for (auto body_id : bodies)
		{
			Entity* entity = (Entity*)body_interface.GetUserData(body_id);
			entity->position = Convert(body_interface.GetPosition(body_id));
			entity->rotation = ConvertQuat(body_interface.GetRotation(body_id));
		}
	}
}

PhysicsShape* PhysicsCreateBoxShape(float3 size)
{
	PhysicsShape* shape = new PhysicsShape();
	char name[64];
	snprintf(name, sizeof(name), "BoxShape_%.1f_%.1f_%.1f", size.x, size.y, size.z);
	AssetInit(shape, AssetType_PhysicsShape, name);

	JPH::BoxShapeSettings floor_shape_settings(ConvertSize(size));
	floor_shape_settings.SetEmbedded();

	auto shape_ref = floor_shape_settings.Create().Get();
	shape->shape = shape_ref.Disown();
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
		shape->shape, Convert(entity->position), {0,0,0,1}, motion_type, layer);

	JPH::Body* body = body_interface.CreateBody(body_creation_settings);
	body_interface.AddBody(body->GetID(), JPH::EActivation::Activate);

	body->SetUserData((U64)entity);
	entity->body = body;
}

void PhysicsDebugDraw(Physics* physics)
{
	ZoneScoped;

	physics->system.DrawBodies(JPH::BodyManager::DrawSettings{
		.mDrawShape = true,
		.mDrawShapeWireframe = true,
	}, &debug_renderer);
}