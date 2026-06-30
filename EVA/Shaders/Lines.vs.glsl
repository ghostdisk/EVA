layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec4 a_Color;

layout (location = 0) out vec4 v_Color;

// --- uniform buffers ----------------------------------------

layout (std140, binding = 0) uniform MainConstantBuffer
{
	mat4 view;
	mat4 view_projection;
} u_main;

// ------------------------------------------------------------

void main()
{
	gl_Position = u_main.view_projection * vec4(a_Position, 1);
	v_Color = a_Color;
}