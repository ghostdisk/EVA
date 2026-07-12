#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
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

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec2 v_Texcoord;

#ifdef S_BRUSH
layout(location = 2) out vec3 v_WorldPos;
#endif

void main() {
	MeshVertex vertex = bindlessBuffers_MeshVertex[nonuniformEXT(push.vertexBuffer)].data[gl_VertexIndex];

	mat4 viewProjection = bindlessBuffers_FrameGlobals[nonuniformEXT(push.cameraBuffer)].data.viewProjection;
	vec4 worldPosition = push.model * vec4(vertex.position, 1.0);
	gl_Position = viewProjection * worldPosition;

	v_Normal = (push.model * vec4(vertex.normal, 0.0)).xyz;
	v_Texcoord = vertex.texcoord;

#ifdef S_BRUSH
	v_WorldPos = worldPosition.xyz / worldPosition.w;
#endif
}
