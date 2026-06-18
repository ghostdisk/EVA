#include <EVA/Physics.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Renderer/DebugRenderer.h>

struct Physics
{
	JPH::PhysicsSystem  system;
	JPH::TempAllocatorImpl* temp_allocator;
};

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

static inline JPH::Quat ConvertQuat(float4 quat)
{
	return JPH::Quat(
		 quat.x,
		 quat.z,
		-quat.y,
		 quat.w);
}