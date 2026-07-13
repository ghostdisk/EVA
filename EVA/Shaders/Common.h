#ifndef EVA_SHADER_COMMON_H
#define EVA_SHADER_COMMON_H

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

struct FrameGlobals {
	mat4 view;
	mat4 viewProjection;
};

struct MeshVertex {
	vec3 position;
	vec3 normal;
	vec2 texcoord;
};

struct LineVertex {
	vec3 position;
	vec4 color;
};

struct DrawQuad {
	int mode;
	int textureId;
	int samplerId;
	int pad2;
	vec4 positionRect;
	vec4 uvRect;
	vec4 tint;
};

layout(set = 0, binding = 0, scalar) readonly buffer BindlessBuffer_FrameGlobals {
	FrameGlobals data;
} bindlessBuffers_FrameGlobals[];

layout(set = 0, binding = 0, scalar) readonly buffer BindlessBuffer_MeshVertex {
	MeshVertex data[];
} bindlessBuffers_MeshVertex[];

layout(set = 0, binding = 0, scalar) readonly buffer BindlessBuffer_LineVertex {
	LineVertex data[];
} bindlessBuffers_LineVertex[];

layout(set = 0, binding = 0, scalar) readonly buffer BindlessBuffer_DrawQuad {
	DrawQuad data[];
} bindlessBuffers_DrawQuad[];

layout(set = 0, binding = 1) uniform texture2D bindlessImages[];
layout(set = 0, binding = 2) uniform sampler bindlessSamplers[];

#endif
