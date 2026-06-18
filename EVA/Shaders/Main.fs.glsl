#version 430 core

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec3 v_Normal;
layout (location = 1) in vec2 v_Texcoord;

layout (location = 1) uniform sampler2D u_Texture;

void main()
{
	vec3 normal = normalize(v_Normal);

	float light1 = dot(v_Normal, normalize(vec3(-1, -.5, .7)));

	// if (light1 < 0.0) light1 = 0.0;
	light1 = light1 * 0.5 + 0.5;

	float light = light1 * 0.7 + 0.3;
	
	vec3 albedo =texture(u_Texture, v_Texcoord).xyz;
	o_Color = vec4(albedo * light, 1);
}