#include <EVA/Math.hpp>
#include <math.h>
#include <cglm/mat4.h>

float4 operator*(const float4x4& mat, const float4& p)
{
	float4 out;
	glm_mat4_mulv(mat, p, out);
	return out;
}

float Intersect(const Ray& ray, const Plane& plane)
{
    float denom = Dot(plane.normal, ray.direction);
    if (fabsf(denom) < 1e-6f)
	{
        return -1;
	}
    return (plane.distance - Dot(plane.normal, ray.origin)) / denom;
}

Plane operator*(const float4x4& mat, const Plane& plane0)
{
	float4 p0 = float4(plane0.normal * plane0.distance, 1);
	float3 p1 = (mat * p0).xyz();
	float3 n1 = (mat * float4(plane0.normal, 0)).xyz().Normalized();
	float d1 = Dot(n1, p1);
	return Plane(n1, d1);
}