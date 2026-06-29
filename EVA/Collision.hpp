#pragma once
#include <EVA/Math.hpp>

float Intersect(const Ray& ray, const Plane& plane);
float DistanceToLineSegment(const Ray& ray, const float3& p1, const float3& p2, float* out_t1 = nullptr, float* out_t2 = nullptr);
float2 NearestPointToLineSegment(float2 a, float2 b, float2 point);

float3 ClosestPtPlanePoint(const Plane& plane, const float3& point);
float DistPlanePoint(const Plane& plane, const float3& point);
void ClosestPtPointSegment(const float3& c, const float3& a, const float3& b, float* out_t, float3* out_d);