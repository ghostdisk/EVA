#include <EVA/Physics.hpp>
#include <EVA/Physics_Jolt.hpp>
#include <EVA/Renderer/GL.hpp> // MeshVertex
#include <stdio.h>
#include <stdarg.h>
#include <thread>

static void TraceImpl(const char *fmt, ...) {
	char buffer[1024];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	printf("Jolt trace: %s\n", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, JPH::uint inLine) {
	printf("Jolt assert: %s:%d: (%s): %s\n", inFile, inLine, inExpression, (inMessage != nullptr? inMessage : ""));
	return true;
};
#endif // JPH_ENABLE_ASSERTS

namespace Layers {
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

struct ObjectLayerPairFilterImpl : JPH::ObjectLayerPairFilter {
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
		switch (inObject1) {
			case Layers::NON_MOVING: return inObject2 == Layers::MOVING; 
			case Layers::MOVING: return true; 
			default: JPH_ASSERT(false); return false;
		}
	}
};

namespace BroadPhaseLayers {
	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH::BroadPhaseLayer MOVING(1);
	static constexpr JPH::uint NUM_LAYERS(2);
};

struct BPLayerInterfaceImpl : JPH::BroadPhaseLayerInterface {
	JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];

	BPLayerInterfaceImpl() {
		mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
	}

	virtual JPH::uint GetNumBroadPhaseLayers() const override {
		return BroadPhaseLayers::NUM_LAYERS;
	}

	virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
		switch ((JPH::BroadPhaseLayer::Type)inLayer) {
			case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
			case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
			default:													    JPH_ASSERT(false); return "INVALID";
		}
	}
#endif
};

struct ObjectVsBroadPhaseLayerFilterImpl : JPH::ObjectVsBroadPhaseLayerFilter {
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
		switch (inLayer1) {
			case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
			case Layers::MOVING: return true;
			default: JPH_ASSERT(false); return false;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////

static JPH::TempAllocatorImpl*             g_temp_allocator;
static JPH::JobSystemThreadPool            g_job_system;
static BPLayerInterfaceImpl                g_broad_phase_layer_interface;
static ObjectVsBroadPhaseLayerFilterImpl   g_object_vs_broadphase_layer_filter;
static ObjectLayerPairFilterImpl           g_object_vs_object_layer_filter;

////////////////////////////////////////////////////////////////////////////////

float3 ConvertPos(const JPH::Vec3& vec) {
	return float3(vec.mF32[0], -vec.mF32[2], vec.mF32[1]);
}

JPH::Vec3 ConvertPos(const float3& vec) {
	return JPH::Vec3(vec.x, vec.z, -vec.y);
}

float4 ConvertQuat(const JPH::Quat& q) {
    return float4(q.mValue.mF32[0], -q.mValue.mF32[2], q.mValue.mF32[1], q.mValue.mF32[3]);
}

JPH::Quat ConvertQuat(const float4& q) {
    return JPH::Quat(q.x, q.z, -q.y, q.w);
}

void PhysicsInitialize() {
	JPH::RegisterDefaultAllocator();
	g_temp_allocator = new JPH::TempAllocatorImpl(10u << 20);
	JPH::Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl);
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();
	g_job_system.Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
}

PhysicsWorld* PhysicsWorldCreate() {
	PhysicsWorld* world = new PhysicsWorld();

	JPH::uint cMaxBodies             = 1024;
	JPH::uint cNumBodyMutexes        = 0;
	JPH::uint cMaxBodyPairs          = 1024;
	JPH::uint cMaxContactConstraints = 1024;
	world->system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, g_broad_phase_layer_interface, g_object_vs_broadphase_layer_filter, g_object_vs_object_layer_filter);

	return world;
}

void PhysicsWorldDestroy(PhysicsWorld* world) {
	delete world;
}

void PhysicsTick(PhysicsWorld* world, double dt) {
	world->system.Update(dt, 1, g_temp_allocator, &g_job_system);
}

PhysicsCollider PhysicsCreateMeshCollider(size_t num_vertices, MeshVertex* vertices, size_t num_indices, U32* indices) {
	JPH::Array<JPH::Float3> inVertices;
	JPH::Array<JPH::IndexedTriangle> inTriangles;

	for (size_t i = 0; i < num_vertices; i++) {
		const MeshVertex& vertex = vertices[i];
		inVertices.push_back(JPH::Float3(vertex.position.x, vertex.position.z, -vertex.position.y));
	}
	for (size_t i = 0; i < num_indices; i += 3) {
		inTriangles.push_back(JPH::IndexedTriangle(indices[i], indices[i + 1], indices[i + 2]));
	}
	
	JPH::MeshShapeSettings settings(inVertices, inTriangles);
	settings.SetEmbedded();

	JPH::Shape* shape = nullptr;
	JPH::Ref<JPH::Shape> shape_ref = settings.Create().Get();
	shape = shape_ref.GetPtr();
	shape->AddRef();
	return PhysicsCollider{ .shape = shape };
}

PhysicsCollider PhysicsCreateBoxCollider(const float3& half_extents) {
	JPH::BoxShapeSettings settings(JPH::Vec3(XYZ(half_extents)));
	settings.SetEmbedded();

	JPH::Shape* shape = nullptr;
	JPH::Ref<JPH::Shape> shape_ref = settings.Create().Get();
	shape = shape_ref.GetPtr();
	shape->AddRef();
	return PhysicsCollider{ .shape = shape };
}

void PhysicsDestroyCollider(PhysicsCollider& collider) {
	collider.shape->Release();
	collider.shape = nullptr;
}

PhysicsBody PhysicsCreateBody(PhysicsWorld* world, PhysicsCollider collider, const float3& position, const float4& rotation, bool is_static) {
	JPH::BodyCreationSettings settings(
		collider.shape,
		ConvertPos(position),
		ConvertQuat(rotation),
		is_static ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
		is_static ? Layers::NON_MOVING : Layers::MOVING);

	JPH::BodyInterface& body_interface = world->system.GetBodyInterfaceNoLock();

	JPH::Body* body = body_interface.CreateBody(settings);
	body_interface.AddBody(body->GetID(), is_static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
	return PhysicsBody{ .body = body };
}

void PhysicsDestroyBody(PhysicsWorld* world, PhysicsBody body) {
	JPH::BodyInterface& body_interface = world->system.GetBodyInterfaceNoLock();
	body_interface.RemoveBody(body.body->GetID());
	body_interface.DestroyBody(body.body->GetID());
}