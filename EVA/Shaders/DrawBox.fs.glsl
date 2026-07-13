#version 460
#include "Common.h"

layout(push_constant) uniform PushConstants {
	vec2 framebufferSize;
	uint quadBuffer;
	uint quadOffset;
	uint vertexBuffer;
	uint textureImage;
	uint textureSampler;
} push;

layout(location = 0) in vec2 v_Texcoord;
layout(location = 1) flat in uint v_Mode;
layout(location = 2) in vec4 v_Tint;

layout(location = 0) out vec4 o_Color;

void main() {
	if (v_Mode == 0) {
		o_Color = v_Tint;
		return;
	}

	vec4 tex = texture(
		sampler2D(
			bindlessImages[nonuniformEXT(push.textureImage)],
			bindlessSamplers[nonuniformEXT(push.textureSampler)]),
		v_Texcoord);

	if (v_Mode == 1) {
		o_Color = vec4(v_Tint.rgb, v_Tint.a * tex.r);
	} else {
		o_Color = tex * v_Tint;
	}
}
