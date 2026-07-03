#include <EVA/Math.hpp>
#include <math.h>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/quat.h>

float4 operator*(const float4x4& mat, const float4& p)
{
	float4 out;
	glm_mat4_mulv(mat, p, out);
	return out;
}

float3 float4x4::TransformPosition(float3 pos) const
{
	float3 out;
	glm_mat4_mulv3(*this, pos, 1, out);
	return out;
}

float4x4 operator*(const float4x4& a, const float4x4& b)
{
	float4x4 out;
	glm_mat4_mul(a, b, out);
	return out;
}


Plane operator*(const float4x4& mat, const Plane& plane0)
{
	float4 p0 = float4(plane0.normal * plane0.distance, 1);
	float3 p1 = (mat * p0).xyz();
	float3 n1 = (mat * float4(plane0.normal, 0)).xyz().Normalized();
	float d1 = Dot(n1, p1);
	return Plane(n1, d1);
}


float4x4 float4x4::FromTransform(const float3& position)
{
	float4x4 m;
	glm_translate_make(m, position);
	return m;
}

float4x4 float4x4::FromTransform(const float3& position, const float4& rotation)
{
	float4x4 m;
	glm_translate_make(m, position);
	glm_quat_rotate(m, rotation, m);
	return m;
}

float4x4 float4x4::FromTransform(const float3& position, const float4& rotation, const float3& scale)
{
	float4x4 m;
	glm_translate_make(m, position);
	glm_quat_rotate(m, rotation, m);
	glm_scale(m, scale);
	return m;
}

float Unlerp(float2 a, float2 m, float2 b)
{
	if (fabs(m.x - a.x) > 0.01f)
	{
		return (m.x - a.x) / (b.x - a.x);
	}
	else
	{
		return (m.y - a.y) / (b.y - a.y);
	}
}

inline bool IntersectAxis(float a_min, float a_max, float b_min, float b_max)
{
	if (b_min >= a_min && b_min < a_max) return true;
	if (a_min >= b_min && a_min < b_max) return true;
	return false;
}

bool Intersect(const AABB& a, const AABB& b)
{
	if (!IntersectAxis(a.min.x, a.max.x, b.min.x, b.max.x)) return false;
	if (!IntersectAxis(a.min.y, a.max.y, b.min.y, b.max.y)) return false;
	if (!IntersectAxis(a.min.z, a.max.z, b.min.z, b.max.z)) return false;
	return true;
}