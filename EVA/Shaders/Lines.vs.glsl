#version 460
#include "Common.h"

layout(push_constant) uniform PushConstants {
	uint cameraBuffer;
	uint vertexBuffer;
	uint vertexOffset;
} push;

layout(location = 0) out vec4 v_Color;

void main() {
	LineVertex vertex = bindlessBuffers_LineVertex[nonuniformEXT(push.vertexBuffer)].data[push.vertexOffset + gl_VertexIndex];
	v_Color = vertex.color;

	mat4 viewProjection = bindlessBuffers_FrameGlobals[nonuniformEXT(push.cameraBuffer)].data.viewProjection;
	gl_Position = viewProjection * vec4(vertex.position, 1.0);
}
