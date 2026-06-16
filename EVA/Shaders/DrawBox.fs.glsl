#version 430 core

layout (location = 0) out vec4 o_Color;

layout (location = 0)      in vec2 v_Texcoord;
layout (location = 1) flat in uint v_Mode;
layout (location = 2)      in vec4 v_Tint;

layout (location = 1) uniform sampler2D u_Texture;

void main()
{
	vec4 tex = texture(u_Texture, v_Texcoord);
	vec4 color;

	if (v_Mode == 0) // solid color
	{
		color = v_Tint;
	}
	else // text
	{
		float alpha = tex.r;
		color = vec4(v_Tint.rgb , v_Tint.a * alpha);
	}

	o_Color = color;
}