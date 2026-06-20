#include <EVA/CSG.hpp>
#include <EVA/GL.hpp>
#include <cglm/vec3.h>
#include <algorithm>

void CSGClearBrush(CSGBrush* brush)
{
	for (CSGPlane& p : brush->planes)
	{
		p.points.clear();
	}
	if (brush->mesh) MeshDestroy(brush->mesh);
}

void CSGBuildBrush(CSGBrush* brush)
{
	CSGClearBrush(brush);

	for (int i = 0; i < brush->planes.size(); i++)
	{
		CSGPlane& a = brush->planes[i];

		for (int j = i + 1; j < brush->planes.size(); j++)
		{
			CSGPlane& b = brush->planes[j];
			float3 c1 = Cross(a.plane.normal, b.plane.normal);

			for (int k = j + 1; k < brush->planes.size(); k++)
			{
				CSGPlane& c = brush->planes[k];
				float det = Dot(c.plane.normal, c1);

				if (abs(det) > 0.0001)
				{
					float3 c2 = Cross(b.plane.normal, c.plane.normal);
					float3 c3 = Cross(c.plane.normal, a.plane.normal);

					float3 p = (c1 * c.plane.distance + c2 * a.plane.distance + c3 * b.plane.distance) / det;

					bool culled = false;
					for (CSGPlane& plane : brush->planes)
					{
						float d = Dot(plane.plane.normal, p);
						if (d > plane.plane.distance + 0.0001)
						{
							culled = true;
							break;
						}
					}
					if (!culled)
					{
						a.points.push_back(p);
						b.points.push_back(p);
						c.points.push_back(p);
					}
				}
			}
		}
	}

	// Remove planes that don't contribute to the volume:
	for (int i = 0; i < brush->planes.size(); i++)
	{
		CSGPlane& plane = brush->planes[i];
		if (plane.points.size() < 3)
		{
			brush->planes[i] = brush->planes.back();
			brush->planes.pop_back();
			i--;
		}
	}

	brush->dirty = false;
}

void CSGBuildBrushMesh(CSGBrush* brush)
{
	std::vector<MeshVertex> vertices;
	std::vector<U32> indices;

	for (int i = 0; i < brush->planes.size(); i++)
	{
		CSGPlane& plane = brush->planes[i];
		if (plane.points.size() < 3)
		{
			continue;
		}

		// construct a 2D basis in the plane:
		float3 n = plane.plane.normal;
		float3 ref = abs(n.x) > 0.9 ? float3(0, 1, 0) : float3(1, 0, 0);
		float3 u = Cross(ref, n).Normalized();
		float3 v = Cross(n, u);	

		// find the center:
		float3 center = {};
		for (const float3& p : plane.points)
		{
			center += p;
		}
		center /= plane.points.size();

		// sort points CCW around center:
		std::sort(plane.points.begin(), plane.points.end(),
			[&](const float3& a, const float3& b)
			{
				float3 da = a - center;
				float3 db = b - center;
				return atan2f(Dot(da, v), Dot(da, u)) < atan2f(Dot(db, v), Dot(db, u));
			});

		// triangulate:
		U32 vertex_start = vertices.size();
		for (const float3& p : plane.points)
		{
			vertices.push_back(MeshVertex{
				.position = p,
				.normal = plane.plane.normal,
				.texcoord = {},
			});
		}
		for (int i = 2; i < plane.points.size(); i++)
		{
			indices.push_back(vertex_start);
			indices.push_back(vertex_start + i - 1);
			indices.push_back(vertex_start + i);
		}
	}
	brush->mesh = MeshCreate("Brush", vertices.size(), vertices.data(), indices.size(), indices.data());
}

CSGBrush* CSGCloneBrush(CSGBrush* orig)
{
	CSGBrush* copy = CSGCreateBrush();
	copy->planes = orig->planes;
	copy->dirty = orig->dirty;
	return copy;
}

void CSGDifference(CSGBrush* a, CSGBrush* b, std::vector<CSGBrush*>& out)
{
	assert(!a->dirty && !b->dirty);
	CSGBrush* cut1 = CSGCloneBrush(a);

	for (CSGPlane& plane : b->planes)
	{
		CSGBrush* cut2 = CSGCloneBrush(cut1);

		cut1->planes.push_back({ .plane = plane.plane });
		cut2->planes.push_back({ .plane = plane.plane.Invert()});
		CSGBuildBrush(cut1);
		CSGBuildBrush(cut2);

		if (cut2->planes.size()) out.push_back(cut2);
		else CSGDestroyBrush(cut2);

		if (!cut1->planes.size())
		{
			break;
		}
	}
	CSGDestroyBrush(cut1);
}

void CSGBuildStack(CSGStack* stack)
{
	stack->dirty = false;

	for (CSGBrush* brush : stack->built_brushes)
	{
		CSGDestroyBrush(brush);
	}
	stack->built_brushes.clear();

	for (const CSGStackNode& node : stack->nodes)
	{
		switch (node.type)
		{
			case CSGStackNodeType_Brush:
			{
				if (node.brush->dirty) CSGBuildBrush(node.brush);
				std::vector<CSGBrush*> out;

				switch (node.operation)
				{
					case CSGOperation_Union:
					{
						assert(stack->built_brushes.size() == 0);
						assert(!node.brush->dirty);
						if (node.brush->dirty) CSGBuildBrush(node.brush);
						stack->built_brushes.push_back(CSGCloneBrush(node.brush));
						break;
					}
					case CSGOperation_Difference:
					{
						std::vector<CSGBrush*> new_brushes;
						for (CSGBrush* old : stack->built_brushes)
						{
							CSGDifference(old, node.brush, new_brushes);
							CSGDestroyBrush(old);
						}
						stack->built_brushes = new_brushes;
						break;
					}
					case CSGOperation_Intersection:
					{
						assert(0);
						break;
					}
				}


				break;
			}
			default:
			{
				assert(!"Not implemented");
			}
		}
	}
}

CSGBrush* CSGCreateCube(float3 size)
{
	CSGBrush* brush = CSGCreateBrush();
	brush->planes.push_back({ Plane(float3( 1, 0, 0), size.x) });
	brush->planes.push_back({ Plane(float3( -1, 0, 0), size.x) });
	brush->planes.push_back({ Plane(float3(0,  1, 0), size.y) });
	brush->planes.push_back({ Plane(float3(0,  -1, 0), size.y) });
	brush->planes.push_back({ Plane(float3(0, 0,  1), size.z) });
	brush->planes.push_back({ Plane(float3(0, 0,  -1), size.z) });
	return brush;
}

CSGBrush* CSGCreateBrush()
{
	CSGBrush* brush = new CSGBrush();
	brush->dirty = true;
	return brush;
}

void CSGDestroyBrush(CSGBrush* brush)
{
	if (brush->mesh) MeshDestroy(brush->mesh);
	delete brush;
}

CSGStack* CSGCreateStack()
{
	CSGStack* stack = new CSGStack();
	return stack;
}

void CSGDestroyStack(CSGStack* stack)
{
	delete stack;
}