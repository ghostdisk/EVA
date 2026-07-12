#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#include "Common.h"

layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

void main() {
	o_Color = v_Color;
}
