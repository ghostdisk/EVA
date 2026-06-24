#include <EVA/Common.hpp>
#include <EVA/Math.hpp>
#include <vector>

struct Mesh;
struct CSGPlane;
struct CSGStack;
struct CSGBrush;
struct UIContext;

struct CSGPlane
{
	Plane               plane   = {};
	std::vector<float3> points  = {};
};

struct CSGBrush
{
	std::vector<CSGPlane> planes  = {};
	Mesh*                 mesh    = nullptr;
	bool                  dirty   = true;
};

enum CSGStackNodeType
{
	CSGStackNodeType_None,
	CSGStackNodeType_Brush,
	CSGStackNodeType_Stack,
};

enum CSGOperation
{
	CSGOperation_Union,
	CSGOperation_Difference,
	CSGOperation_Intersection,
};

struct CSGStackNode
{
	CSGStackNodeType  type      = CSGStackNodeType_None;
	float4x4          transform = float4x4::Identity();
	CSGOperation      operation = CSGOperation_Union;
	union
	{
		CSGBrush* brush;
		CSGStack* stack;
	};
};

struct CSGStack
{
	bool                      dirty            = true;
	std::vector<CSGStackNode> nodes            = {};
	std::vector<CSGBrush*>    built_brushes    = {};
};

CSGBrush*   CSGCreateBrush();
void        CSGDestroyBrush(CSGBrush* brush);
void        CSGBuildBrushMesh(CSGBrush* brush);
void        CSGBuildBrush(CSGBrush* brush);
CSGBrush*   CSGCloneBrush(CSGBrush* orig);

CSGStack*   CSGCreateStack();
void        CSGDestroyStack(CSGStack* stack);
void        CSGBuildStack(CSGStack* stack);

CSGBrush*   CSGCreateCube(float3 size);
CSGBrush*   CSGCreateCylinder(int segments, float radius, float height);

float Intersect(const Ray& ray, CSGBrush* brush);