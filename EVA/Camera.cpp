#include <EVA/Camera.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Entities.hpp>
#include <EVA/Input.hpp>
#include <SDL3/SDL.h>
#include <cglm/clipspace/persp_rh_no.h>
#include <cglm/clipspace/view_rh_no.h>
#include <cglm/euler.h>

void CameraInit(Camera& camera)
{
	camera.position = {};
	camera.forward = {0, 1, 0};
	camera.up = {0, 0, 1};
}

void CameraUpdateMatrices(Camera& camera)
{
	CameraUpdateBasisVectors(camera);

	glm_look_rh_no(camera.position, camera.forward, camera.up, camera.view_matrix);
	glm_perspective_rh_no(camera.fov, (float)WindowWidth / (float)WindowHeight, 0.1f, 500.0f, camera.projection_matrix);
	glm_mat4_mul(camera.projection_matrix, camera.view_matrix, camera.view_projection_matrix);
}

void CameraUpdateBasisVectors(Camera& camera)
{
	float4x4 mat;
	float euler_angles[3] = {camera.pitch, 0, camera.yaw};

	glm_euler_zxy(euler_angles, mat);
	camera.right   = float3(mat.data[0][0], mat.data[0][1], mat.data[0][2]);
	camera.up      = float3(mat.data[2][0], mat.data[2][1], mat.data[2][2]);
	camera.forward = float3(mat.data[1][0], mat.data[1][1], mat.data[1][2]);
}

void CameraFly(Camera& camera)
{
	float3 input = {
		InputGetAxis(InputAxis_Horizontal),
		InputGetAxis(InputAxis_Vertical),
		0,
	};

	if (input.x || input.y) input = input.Normalized();

	if (InputGetButton(INPUT_BUTTON_MOUSE_RIGHT))
	{
		camera.yaw   -= InputGetAxis(InputAxis_MouseX) * camera.mouse_sensitivity.x;
		camera.pitch -= InputGetAxis(InputAxis_MouseY) * camera.mouse_sensitivity.y;
	}

	float speed = InputGetButton(SDL_SCANCODE_LSHIFT) ? camera.fly_speed_sprint : camera.fly_speed;
	if (InputGetButton(SDL_SCANCODE_LCTRL)) speed = camera.fly_speed_slow;

	input = camera.forward * input.y + camera.right * input.x + camera.up * input.z;
	input.z += InputGetAxis(InputAxis_Fly);

	camera.position += input * speed * DeltaTime;
	camera.yaw = remainderf(camera.yaw, GLM_PIf * 2);

	float yaw_deg = camera.yaw * RAD_TO_DEG;
	const char* axis;
	if (yaw_deg > -45 && yaw_deg < 45) axis = "+y";
	else if (yaw_deg > 45 && yaw_deg < 135) axis = "-x";
	else if (yaw_deg > 135) axis = "-y";
	else axis = "+x";
	LogToScreen("Cam %.1f %.1f %.1f Facing %.1f (%s)", camera.position.x, camera.position.y, camera.position.z, yaw_deg, axis);
}

void CameraOrbit(Camera& camera, Entity* entity)
{
	SDL_SetWindowRelativeMouseMode(GameWindow, !InMenu);

	if (!InMenu)
	{
		camera.yaw   -= InputGetAxis(InputAxis_MouseX) * camera.mouse_sensitivity.x;
		camera.pitch -= InputGetAxis(InputAxis_MouseY) * camera.mouse_sensitivity.y;

		if (camera.pitch < -90 * DEG_TO_RAD)
		{
			camera.pitch = -90 * DEG_TO_RAD;
		}
		if (camera.pitch > 45 * DEG_TO_RAD)
		{
			camera.pitch = 45 * DEG_TO_RAD;
		}

		if (InputGetButton(SDL_SCANCODE_KP_1)) camera.orbit_distance += 3 * DeltaTime;
		if (InputGetButton(SDL_SCANCODE_KP_2)) camera.orbit_distance -= 3 * DeltaTime;

		CameraUpdateBasisVectors(camera);

		camera.position = entity->position;
		camera.position.z += camera.orbit_height;
		camera.position -= camera.forward * camera.orbit_distance;
	}
}
