#version 430 core

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec3 v_Normal;
layout (location = 1) in vec2 v_Texcoord;

void main()
{
	vec3 normal = normalize(v_Normal);
	o_Color = vec4(normal * 0.5 + 0.5, 1);
}