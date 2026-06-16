#version 430 core

layout (location = 0) in vec3 a_Position;
layout (location = 0) out vec2 v_Texcoord;

void main()
{
	gl_Position = vec4(a_Position, 1);
	v_Texcoord = a_Position.xy;
}