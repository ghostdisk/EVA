#version 460
#include "Common.h"

layout(push_constant) uniform PushConstants {
	mat4 model;
	vec4 color;
	float textureScale;
	uint cameraBuffer;
	uint vertexBuffer;
	uint colorImage;
	uint colorSampler;
} push;

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec2 v_Texcoord;

#ifdef S_BRUSH
layout(location = 2) in vec3 v_WorldPos;
#endif

layout(location = 0) out vec4 o_Color;

vec4 SampleColor(vec2 uv) {
	return texture(
		sampler2D(
			bindlessImages[nonuniformEXT(push.colorImage)],
			bindlessSamplers[nonuniformEXT(push.colorSampler)]),
		uv);
}

void main() {
	vec3 normal = normalize(v_Normal);
	float light1 = dot(normal, normalize(vec3(-1.0, -0.5, 0.7)));
	light1 = light1 * 0.5 + 0.5;
	float light = light1 * 0.7 + 0.3;

	vec3 albedo;
#ifdef S_BRUSH
	vec3 tx = SampleColor(v_WorldPos.yz * push.textureScale).xyz;
	vec3 ty = SampleColor(v_WorldPos.xz * push.textureScale).xyz;
	vec3 tz = SampleColor(v_WorldPos.xy * push.textureScale).xyz;

	vec3 blend = abs(normal);
	blend /= blend.x + blend.y + blend.z;
	albedo = tx * blend.x + ty * blend.y + tz * blend.z;
#else
	albedo = SampleColor(v_Texcoord * push.textureScale).xyz;
#endif

	albedo *= push.color.xyz;
	o_Color = vec4(albedo * light, 1.0);
}
