#pragma once
#include <EVA/Core/Basic.hpp>
#include <EVA/Math.hpp>

class Mesh;
struct CSGPlane;
struct CSGBrush;
struct UIContext;
struct EdOp;

struct CSGPlane {
	Plane          plane   = {};
	Vector<float3> points  = {};
};

struct CSGBrush {
	Vector<CSGPlane>   planes  = {};
	Mesh*              mesh    = nullptr;
	AABB               aabb    = {};

	// userdata
	EdOp* sources[2] = {};
};

CSGBrush*   CSGCreateBrush();
void        CSGDestroyBrush(CSGBrush* brush);
void        CSGBuildBrushMesh(CSGBrush* brush);
void        CSGBuildBrush(CSGBrush* brush);
CSGBrush*   CSGCloneBrush(CSGBrush* orig);
void        CSGDifference(CSGBrush* a, CSGBrush* b, Vector<CSGBrush*>& out);
void        CSGBrushTransform(CSGBrush* brush, const float4x4& transform);

CSGBrush*   CSGCreateCube(float3 size);
CSGBrush*   CSGCreateCylinder(int segments, float radius, float height);

float Intersect(const Ray& ray, CSGBrush* brush, const float4x4& transform, int* out_plane = nullptr);