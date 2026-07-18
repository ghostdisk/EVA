#pragma once
#include <EVA/Math.hpp>
#include <EVA/Entities/Entity.hpp>

struct Entity;

class ECamera : public Entity {
public:
	ECLASS_COMMON(ECamera);

	float  fov       = 60 * GLM_PI / 180.0f;
	float  pitch     = 0.0f;
	float  yaw       = 0.0f; // TODO: yaw=0 should point to the right, not forward

	// calculated from pitch/yaw by CameraUpdateBasisVectors:
	float3 forward  = {};
	float3 right    = {};
	float3 up       = {};

	// calculated by CameraUpdateMatrices:
	float4x4 view_matrix                    = {};
	float4x4 projection_matrix              = {};
	float4x4 view_projection_matrix         = {};
	float4x4 inverse_view_projection_matrix = {};

	// CameraFly() parameters:
	float  fly_speed         = 10.0f;
	float  fly_speed_sprint  = 100.0f;
	float  fly_speed_slow    = 0.3f;
	float2 mouse_sensitivity = { 0.003f, 0.003f };

	// Orbit camera parameters:
	float orbit_height = 1.5f;
	float orbit_distance = 4.0f;
};

void CameraInit                (ECamera& camera);
void CameraUpdateBasisVectors  (ECamera& camera);
void CameraUpdateMatrices      (ECamera& camera);
void CameraFly                 (ECamera& camera);
void CameraOrbit               (ECamera& camera, Entity* entity);

Ray CameraClipToRay(ECamera& camera, float2 clip_xy);
Ray CameraScreenToRay(ECamera& camera, float2 screen_xy);
float3 CameraWorldToScreen(ECamera& camera, float3 world);
