#pragma once
#include <EVA/Math.hpp>
#include <EVA/Math.hpp>

struct Entity;

struct Camera
{
	float fov = 60 * GLM_PI / 180.0f;

	float3 position = {};
	float  pitch    = 0.0f;
	float  yaw       = 0.0f;

	// calculated from pitch/yaw by CameraUpdateBasisVectors:
	float3 forward  = {};
	float3 right    = {};
	float3 up       = {};

	// calculated by CameraUpdateMatrices:
	float4x4 view_matrix            = {};
	float4x4 projection_matrix      = {};
	float4x4 view_projection_matrix = {};

	// CameraFly() parameters:
	float  fly_speed         = 10.0f;
	float  fly_speed_sprint  = 100.0f;
	float2 mouse_sensitivity = { 0.003f, 0.003f };

	// Orbit camera parameters:
	float orbit_height = 1.5f;
	float orbit_distance = 4.0f;
};

void CameraInit(Camera& camera);
void CameraUpdateBasisVectors(Camera& camera);
void CameraUpdateMatrices(Camera& camera);
void CameraFly(Camera& camera);
void CameraOrbit(Camera& camera, Entity* entity);