#version 430 core

layout (location = 0) in vec3 a_Position;
layout (location = 0) out vec2 v_Texcoord;

layout (location = 0) uniform vec2 u_Framebuffer;

struct Quad
{
	vec4 out_rect;
};

layout (std430, binding = 0) buffer Quads
{
	Quad quads[];
};

void main()
{

	vec4 rect = vec4(0, 100, 200, 100);

	vec2 mask = a_Position.xy;

	vec2 pos = rect.xy + mask * rect.zw;
	pos = (pos / u_Framebuffer) * 2.0 - 1.0;
	pos.y = -pos.y;


	gl_Position = vec4(pos, 0, 1);

	v_Texcoord = a_Position.xy;
}