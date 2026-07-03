#include <EVA/Math.hpp>
#include <math.h>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/quat.h>
#include <cglm/ray.h>

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

float Intersect(const Ray& ray, const Plane& plane)
{
    float denom = Dot(plane.normal, ray.direction);
    if (fabsf(denom) < 1e-6f)
	{
        return -1;
	}
    return (plane.distance - Dot(plane.normal, ray.origin)) / denom;
}

float IntersectTriangle(const Ray& ray, const float3& p1, const float3& p2, const float3& p3)
{
	float d = -1.0f;
	if (glm_ray_triangle(ray.origin, ray.direction, p1, p2, p3, &d))
	{
		return d;
	}
	else
	{
		return -1.0f;
	}
}

float DistanceToLineSegment(const Ray& ray, const float3& p1, const float3& p2, float* out_t1, float* out_t2)
{
    float3 u  = ray.direction;            // assumed unit length
    float3 v  = (p2 - p1).Normalized();   // == your pd
    float3 w0 = ray.origin - p1;

    float b = Dot(u, v);                  // cos(angle between lines)
    float d = Dot(u, w0);
    float e = Dot(v, w0);
    float denom = 1.0f - b * b;           // sin^2(angle); ==0 when parallel

    float t1, t2;
    if (denom > 1e-6f) {
        t1 = (b * e - d) / denom;         // along the ray
        t2 = (e - b * d) / denom;         // along the segment, from p1
    } else {
        t1 = 0.0f;                        // parallel: any t1 works, pick origin
        t2 = e;                           // project origin onto the segment line
    }

    float3 A = ray.origin + u * t1;
    float3 B = p1 + v * t2;

    if (out_t1) *out_t1 = t1;
    if (out_t2) *out_t2 = t2;
    return (A - B).Length();
}

float DistanceRayToLine(const Ray& ray, const float3& a, const float3& b, float* out_t1, float* out_t2)
{
    float3 u  = ray.direction;        // assumed unit length
    float3 v  = (b - a).Normalized(); // line direction
    float3 w0 = ray.origin - a;

    float bDot = Dot(u, v);           // cos(angle between lines)
    float d = Dot(u, w0);
    float e = Dot(v, w0);
    float denom = 1.0f - bDot * bDot; // sin^2(angle); ==0 when parallel

    float t1, t2;
    if (denom > 1e-6f) {
        t1 = (bDot * e - d) / denom;  // along the ray
        t2 = (e - bDot * d) / denom;  // along the line, from a
    } else {
        t1 = 0.0f;                    // parallel: any t1 works, pick origin
        t2 = e;                       // project ray origin onto the line
    }

    float3 A = ray.origin + u * t1;
    float3 B = a + v * t2;

    if (out_t1) *out_t1 = t1;
    if (out_t2) *out_t2 = t2;

    return (A - B).Length();
}

float2 NearestPointToLineSegment(float2 a, float2 b, float2 p)
{
	float2 ab = (b - a);
	ab /= ab.Length();
	float t = Dot(ab, p - a);
	return a + ab * t;
}

float DistPlanePoint(const Plane& plane, const float3& point)
{
	return plane.distance - Dot(point, plane.normal);
}