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

layout(location = 0) out vec2 v_Texcoord;
layout(location = 1) flat out uint v_Mode;
layout(location = 2) out vec4 v_Tint;

void main() {
	vec2 mask = bindlessBuffers_MeshVertex[nonuniformEXT(push.vertexBuffer)].data[gl_VertexIndex].position.xy;
	DrawQuad quad = bindlessBuffers_DrawQuad[nonuniformEXT(push.quadBuffer)].data[push.quadOffset + gl_InstanceIndex];

	vec2 position = quad.positionRect.xy + mask * quad.positionRect.zw;
	position = position / push.framebufferSize * 2.0 - 1.0;
	position.y = -position.y;

	gl_Position = vec4(position, 0.0, 1.0);
	v_Texcoord = quad.uvRect.xy + mask * quad.uvRect.zw;
	v_Mode = uint(quad.mode);
	v_Tint = quad.tint;
}
