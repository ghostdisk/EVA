#include <EVA/Camera.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Entities.hpp>
#include <EVA/Input.hpp>
#include <EVA/Console.hpp>
#include <SDL3/SDL.h>
#include <cglm/clipspace/persp_rh_zo.h>
#include <cglm/clipspace/view_rh_zo.h>
#include <cglm/euler.h>
#include <cglm/mat4.h>

Camera* g_current_camera = nullptr;

void CameraInit(Camera& camera) {
	camera.position = {};
	camera.forward = {0, 1, 0};
	camera.up = {0, 0, 1};
}

void CameraUpdateMatrices(Camera& camera) {
	CameraUpdateBasisVectors(camera);

	glm_look_rh_zo(camera.position, camera.forward, camera.up, camera.view_matrix);
	glm_perspective_rh_zo(camera.fov, g_window_size.x / g_window_size.y, 0.02f, 500.0f, camera.projection_matrix);
	glm_mat4_mul(camera.projection_matrix, camera.view_matrix, camera.view_projection_matrix);

	glm_mat4_inv(camera.view_projection_matrix, camera.inverse_view_projection_matrix);
}

void CameraUpdateBasisVectors(Camera& camera) {
	float4x4 mat;
	float euler_angles[3] = {camera.pitch, 0, camera.yaw};

	glm_euler_zxy(euler_angles, mat);
	camera.right   = float3(mat.data[0][0], mat.data[0][1], mat.data[0][2]);
	camera.up      = float3(mat.data[2][0], mat.data[2][1], mat.data[2][2]);
	camera.forward = float3(mat.data[1][0], mat.data[1][1], mat.data[1][2]);
}

void CameraFly(Camera& camera) {
	float3 input = {
		cvar_right.fvalue - cvar_left.fvalue,
		cvar_forward.fvalue - cvar_back.fvalue,
		0,
	};

	if (input.x || input.y) input = input.Normalized();

	if (InputGetButton(INPUT_BUTTON_MOUSE_RIGHT)) {
		camera.yaw   -= g_mouse_delta.x * camera.mouse_sensitivity.x;
		camera.pitch -= g_mouse_delta.y * camera.mouse_sensitivity.y;
	}

	float speed = InputGetButton(SDL_SCANCODE_LSHIFT) ? camera.fly_speed_sprint : camera.fly_speed;
	if (InputGetButton(SDL_SCANCODE_LALT)) speed = camera.fly_speed_slow;

	input = camera.forward * input.y + camera.right * input.x + camera.up * input.z;
	input.z += cvar_flyup.fvalue - cvar_flydown.fvalue;

	camera.position += input * speed * g_delta_time;
	camera.yaw = remainderf(camera.yaw, GLM_PIf * 2);

	float yaw_deg = camera.yaw * RAD_TO_DEG;
	const char* axis;
	if (yaw_deg > -45 && yaw_deg < 45) axis = "+y";
	else if (yaw_deg > 45 && yaw_deg < 135) axis = "-x";
	else if (yaw_deg > 135) axis = "-y";
	else axis = "+x";

	// LogToScreen("Cam %.1f %.1f %.1f Facing %.1f (%s)", camera.position.x, camera.position.y, camera.position.z, yaw_deg, axis);
}

void CameraOrbit(Camera& camera, Entity* entity) {
	bool InMenu = false; // TODO
	SDL_SetWindowRelativeMouseMode(g_game_window, !InMenu);

	if (!InMenu) {
		camera.yaw   -= g_mouse_delta.x * camera.mouse_sensitivity.x;
		camera.pitch -= g_mouse_delta.y * camera.mouse_sensitivity.y;

		if (camera.pitch < -90 * DEG_TO_RAD) {
			camera.pitch = -90 * DEG_TO_RAD;
		}
		if (camera.pitch > 45 * DEG_TO_RAD) {
			camera.pitch = 45 * DEG_TO_RAD;
		}

		if (InputGetButton(SDL_SCANCODE_KP_1)) camera.orbit_distance += 3 * g_delta_time;
		if (InputGetButton(SDL_SCANCODE_KP_2)) camera.orbit_distance -= 3 * g_delta_time;

		CameraUpdateBasisVectors(camera);

		camera.position = entity->position;
		camera.position.z += camera.orbit_height;
		camera.position -= camera.forward * camera.orbit_distance;
	}
}

Ray CameraClipToRay(Camera& camera, float2 pos) {
	float4 clip0 = { XY(pos), 0, 1 };
	float4 clip1 = { XY(pos), 1, 1 };

	float4 world0;
	float4 world1;

	glm_mat4_mulv(camera.inverse_view_projection_matrix, clip0, world0);
	glm_mat4_mulv(camera.inverse_view_projection_matrix, clip1, world1);

	world0.xyz() /= world0.w;
	world1.xyz() /= world1.w;

	return Ray(camera.position, (world1.xyz() - world0.xyz()).Normalized());
}

Ray CameraScreenToRay(Camera& camera, float2 screen) {
	return CameraClipToRay(camera, float2(
		(screen.x / g_window_size.x) * 2.0f - 1.0f,
		-((screen.y / g_window_size.y) * 2.0f - 1.0f)));
}


float3 CameraWorldToScreen(Camera& camera, float3 world) {
	float4 clip = camera.view_projection_matrix * float4(world, 1);
	clip.xyz() /= clip.w;

	return float3(
		((clip.x * 0.5) + 0.5) * g_window_size.x,
		((-clip.y * 0.5) + 0.5) * g_window_size.y,
		clip.z);
}