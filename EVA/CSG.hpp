#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct Mesh;
struct CSGPlane;
struct CSGBrush;
struct UIContext;
struct EdOp;

struct CSGPlane
{
	Plane               plane   = {};
	std::vector<float3> points  = {};
};

struct CSGBrush
{
	std::vector<CSGPlane> planes  = {};
	Mesh*                 mesh    = nullptr;

	// userdata
	EdOp* source = nullptr;
};

CSGBrush*   CSGCreateBrush();
void        CSGDestroyBrush(CSGBrush* brush);
void        CSGBuildBrushMesh(CSGBrush* brush);
void        CSGBuildBrush(CSGBrush* brush);
CSGBrush*   CSGCloneBrush(CSGBrush* orig);
void        CSGDifference(CSGBrush* a, CSGBrush* b, const float4x4& b_transform, std::vector<CSGBrush*>& out);
void        CSGBrushTransform(CSGBrush* brush, const float4x4& transform);

CSGBrush*   CSGCreateCube(float3 size);
CSGBrush*   CSGCreateCylinder(int segments, float radius, float height);

float Intersect(const Ray& ray, CSGBrush* brush, const float4x4& transform, int* out_plane = nullptr);