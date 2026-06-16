#version 430 core

layout (location = 0) in vec3 a_Position;
layout (location = 0) out vec2 v_Texcoord;

layout (location = 0) uniform vec2 u_Framebuffer;

struct UIQuad
{
	vec4 position_rect;
};

layout (std430, binding = 0) buffer Quads
{
	UIQuad quads[];
};

void main()
{
	UIQuad quad = quads[gl_InstanceID];
	vec2 mask = a_Position.xy;

	vec2 position = quad.position_rect.xy + mask * quad.position_rect.zw;
	position = (position / u_Framebuffer) * 2.0 - 1.0;
	position.y = -position.y;

	gl_Position = vec4(position, 0, 1);

	v_Texcoord = a_Position.xy;
}