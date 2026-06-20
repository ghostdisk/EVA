#version 430 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_Texcoord;

layout (location = 0) out vec3 v_Normal;
layout (location = 1) out vec2 v_Texcoord;

layout (location = 0) uniform mat4      u_ViewProjection;
layout (location = 1) uniform sampler2D u_Texture;
layout (location = 2) uniform mat4      u_Model;
layout (location = 3) uniform vec4      u_Tint;

void main()
{
	gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1);
	v_Normal = a_Normal;
	v_Texcoord = a_Texcoord;
}