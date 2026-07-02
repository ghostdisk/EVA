#include <EVA/Collision.hpp>
#include <cglm/ray.h>

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
