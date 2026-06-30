#pragma once
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
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>


struct PhysicsWorld
{
	JPH::PhysicsSystem system;
};

float3    ConvertPos(const JPH::Vec3& vec);
JPH::Vec3 ConvertPos(const float3& vec);
float4    ConvertQuat(const JPH::Quat& quat);
JPH::Quat ConvertQuat(const float4& quat);