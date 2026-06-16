#version 430 core

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec2 v_Texcoord;

layout (location = 1) uniform sampler2D u_Texture;

void main()
{
	o_Color = texture(u_Texture, v_Texcoord);
	// o_Color = vec4(v_Texcoord, 0, 1);
}