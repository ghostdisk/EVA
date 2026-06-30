// --- outputs ------------------------------------------------

layout (location = 0) out vec4 o_Color;

// --- varyings -----------------------------------------------

layout (location = 0) in vec3 v_Normal;
layout (location = 1) in vec2 v_Texcoord;

#ifdef S_BRUSH
layout (location = 2) in vec3 v_WorldPos;
#endif

// --- uniforms -----------------------------------------------

layout (location = 1) uniform sampler2D u_Texture;
layout (location = 2) uniform mat4      u_Model;
layout (location = 3) uniform vec4      u_Tint;

// ------------------------------------------------------------

void main()
{
	vec3 normal = normalize(v_Normal);

	float light1 = dot(v_Normal, normalize(vec3(-1, -.5, .7)));

	light1 = light1 * 0.5 + 0.5;
	float light = light1 * 0.7 + 0.3;
	
	vec3 albedo;

#ifdef S_BRUSH
	vec3 tx = texture(u_Texture, v_WorldPos.yz).xyz;
	vec3 ty = texture(u_Texture, v_WorldPos.xz).xyz;
	vec3 tz = texture(u_Texture, v_WorldPos.xy).xyz;

	vec3 blend = abs(normalize(normal));
	blend /= (blend.x + blend.y + blend.z);

	albedo = tx * blend.x + ty * blend.y + tz * blend.z;
#else
	albedo = texture(u_Texture, v_Texcoord).xyz;
#endif

	albedo *= u_Tint.xyz;
	o_Color = vec4(albedo * light, 1);
}