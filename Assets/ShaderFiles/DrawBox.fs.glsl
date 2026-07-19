#version 460
#include "Common.h"

layout(location = 0) in vec2 v_Texcoord;
layout(location = 1) flat in uint v_Mode;
layout(location = 2) flat in uint v_Texture;
layout(location = 3) flat in uint v_Sampler;
layout(location = 4) in vec4 v_Tint;

layout(location = 0) out vec4 o_Color;

void main() {
	if (v_Mode == 0) {
		o_Color = v_Tint;
		return;
	}

	vec4 tex = texture(
		sampler2D(
			bindlessImages[nonuniformEXT(v_Texture)],
			bindlessSamplers[nonuniformEXT(v_Sampler)]),
		v_Texcoord);

	if (v_Mode == 1) {
		o_Color = vec4(v_Tint.rgb, v_Tint.a * tex.r);
	} else {
		o_Color = tex * v_Tint;
	}
}
