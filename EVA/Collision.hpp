#pragma once
#include <EVA/Math.hpp>

float Intersect(const Ray& ray, const Plane& plane);
float IntersectTriangle(const Ray& ray, const float3& p1, const float3& p2, const float3& p3);
float DistanceToLineSegment(const Ray& ray, const float3& p1, const float3& p2, float* out_t1 = nullptr, float* out_t2 = nullptr);
float2 NearestPointToLineSegment(float2 a, float2 b, float2 point);
