#include <EVA/Camera.hpp>
#include <EVA/Platform.hpp>
#include <EVA/IO.hpp>
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


void CameraFly(Camera& camera)
{
	float3 input = {
		(float)IOGetButton(SDL_SCANCODE_D) - (float)IOGetButton(SDL_SCANCODE_A),
		(float)IOGetButton(SDL_SCANCODE_W) - (float)IOGetButton(SDL_SCANCODE_S),
		0,
	};

	if (input.x || input.y) input = input.Normalized();

	if (IOGetButton(IO_BUTTON_MOUSE_RIGHT))
	{
		camera.yaw   -= IOMouseDelta.x * camera.mouse_sensitivity.x;
		camera.pitch -= IOMouseDelta.y * camera.mouse_sensitivity.y;
	}

	float speed = IOGetButton(SDL_SCANCODE_LSHIFT) ? camera.fly_speed_sprint : camera.fly_speed;

	input = camera.forward * input.y + camera.right * input.x + camera.up * input.z;
	input.z += (float)IOGetButton(SDL_SCANCODE_E) - (float)IOGetButton(SDL_SCANCODE_Q);

	camera.position += input * speed * DeltaTime;
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