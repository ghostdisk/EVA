// --- attributes ---------------------------------------------

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_Texcoord;

// --- varyings -----------------------------------------------

layout (location = 0) out vec3 v_Normal;
layout (location = 1) out vec2 v_Texcoord;

#ifdef S_BRUSH
layout (location = 2) out vec3 v_WorldPos;
#endif

// --- uniform buffers ----------------------------------------

layout (std140, binding = 0) uniform MainConstantBuffer
{
	mat4 view;
	mat4 view_projection;
} u_main;

// --- uniforms -----------------------------------------------

layout (location = 1) uniform sampler2D u_Texture;
layout (location = 2) uniform mat4      u_Model;
layout (location = 3) uniform vec4      u_Tint;
layout (location = 4) uniform float     u_TextureScale;

// ------------------------------------------------------------

void main()
{
	vec4 world_pos = u_Model * vec4(a_Position, 1);
	gl_Position = u_main.view_projection * world_pos;

	v_Normal = (u_Model * vec4(a_Normal, 0)).xyz;
	v_Texcoord = a_Texcoord;

#ifdef S_BRUSH
	v_WorldPos = world_pos.xyz / world_pos.w;
#endif
}