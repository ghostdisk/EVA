#version 430 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec4 a_Color;

layout (location = 0) uniform mat4 u_ViewProjection;

layout (location = 0) out vec4 v_Color;

void main()
{
	gl_Position = u_ViewProjection * vec4(a_Position, 1);
	v_Color = a_Color;
}