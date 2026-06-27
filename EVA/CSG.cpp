#include <EVA/CSG.hpp>
#include <EVA/Renderer/GL.hpp>
#include <EVA/UI.hpp>
#include <cglm/vec3.h>
#include <algorithm>
#include <tracy/Tracy.hpp>

void CSGClearBrush(CSGBrush* brush)
{
	for (CSGPlane& p : brush->planes)
	{
		p.points.clear();
	}
	if (brush->mesh) MeshDestroy(brush->mesh);
}

bool CSGAddPlane(CSGBrush* brush, Plane plane)
{
	ZoneScopedN("CSGAddPlane");
	for (CSGPlane& p : brush->planes)
	{
		if (fabs(plane.distance - p.plane.distance) < 0.001 && Dot(plane.normal, p.plane.normal) > 0.999)
		{
			return false;
		}
	}
	brush->planes.push_back({ .plane = plane });
	return true;
}

// This currently assumes the brush has no duplicate planes!!
void CSGBuildBrush(CSGBrush* brush)
{
	ZoneScopedN("CSGBuildBrush");

	CSGClearBrush(brush);

	{
		// ZoneScopedN("Calculate Corners");

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
							if (d > plane.plane.distance + 0.001)
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
	}

	{
		// ZoneScopedN("Sort points");
		for (CSGPlane& plane : brush->planes)
		{
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
		}
	}

	{
		// ZoneScopedN("Deduplicate points within a plane");
		for (CSGPlane& plane : brush->planes)
		{
			int i = 1, j = 1;
			for (; j < plane.points.size(); j++)
			{
				plane.points[i] = plane.points[j];
				if (Distance(plane.points[i - 1], plane.points[i]) > 0.001)
				{
					i++;
				}
			}
			plane.points.resize(i);
		}
	}

	{
		// ZoneScopedN("Cull planes"); // Remove planes that don't contribute to the volume:

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

		// Remove all planes from zero-volume brushes:
		if (brush->planes.size() < 4)
		{
			brush->planes.clear();
		}
	}
}

void CSGBuildBrushMesh(CSGBrush* brush)
{
	ZoneScopedN("CSGBuildBrushMesh");

	if (brush->mesh)
	{
		MeshDestroy(brush->mesh);
		brush->mesh = nullptr;
	}

	std::vector<MeshVertex> vertices;
	std::vector<U32> indices;

	for (int i = 0; i < brush->planes.size(); i++)
	{
		CSGPlane& plane = brush->planes[i];
		if (plane.points.size() < 3)
		{
			continue;
		}

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
	return copy;
}

CSGBrush* CSGIntersect(CSGBrush* a, CSGBrush* b, const float4x4& b_transform)
{
	CSGBrush* x = CSGCreateBrush();
	if (a->planes.size() && b->planes.size())
	{
		for (CSGPlane& p : a->planes) x->planes.push_back({ .plane = p.plane });
		for (CSGPlane& p : b->planes) x->planes.push_back({ .plane = p.plane * b_transform });
	}
	CSGBuildBrush(x);
	return x;
}

// Takes ownership of a. b is left intact.
void CSGDifference(CSGBrush* a, CSGBrush* b, const float4x4& b_transform, std::vector<CSGBrush*>& out)
{
	bool fully_inside = true;
	CSGBrush* x = CSGIntersect(a, b, b_transform);
	DEFER(CSGDestroyBrush(x));

	for (CSGPlane& csgplane : x->planes)
	{
		Plane plane = csgplane.plane;
		CSGBrush* cut2 = CSGCloneBrush(a);

		CSGAddPlane(a, plane);
		CSGAddPlane(cut2, plane.Invert());
		CSGBuildBrush(a);
		CSGBuildBrush(cut2);

		if (cut2->planes.size())
		{
			fully_inside = false;
			out.push_back(cut2);
		}
		else
		{
			CSGDestroyBrush(cut2);
		}
		if (!a->planes.size())
		{
			break;
		}
	}
	if (!x->planes.size())
	{
		out.push_back(a);
	}
	else if (fully_inside)
	{
		CSGDestroyBrush(a);
	}
}

int HighestBit(unsigned x)
{
	if (x == 0) return -1; 

	int idx = 0;
	while (x >>= 1)
	{
		idx++;
	}
	return idx;
}

U32 NextPow2(U32 x)
{
	if (x <= 1) return 1;
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}

CSGBrush* CSGCreateBrush()
{
	CSGBrush* brush = new CSGBrush();
	return brush;
}

void CSGDestroyBrush(CSGBrush* brush)
{
	if (brush->mesh) MeshDestroy(brush->mesh);
	delete brush;
}

float Intersect(const Ray& ray, CSGBrush* brush, const float4x4& transform, int* out_plane)
{
	float min_t = -1.0f;
	float max_t = INFINITY;
	int min_plane = -1;

	for (int i = 0; i < brush->planes.size(); i++)
	{
		CSGPlane& csgplane = brush->planes[i];
		Plane plane = csgplane.plane * transform;

		float dot = Dot(plane.normal, ray.direction);
		if (abs(dot) > 0.0001)
		{
			float t = Intersect(ray, plane);
			if (dot > 0 && max_t > t)
			{
				max_t = t;
			}
			if (dot < 0 && min_t < t)
			{
				min_plane = i;
				min_t = t;
			}
		}
	}

	if (out_plane) *out_plane = min_plane;
	return min_t < max_t ? min_t : -1.0f;
}

CSGBrush* CSGCreateCylinder(int segments, float radius, float height)
{
	CSGBrush* brush = CSGCreateBrush();
	brush->planes.push_back({ Plane(float3( 0, 0, 1), height/2) });
	brush->planes.push_back({ Plane(float3( 0, 0, -1), height/2) });
	int np = NextPow2(segments);
	int hb = HighestBit(np);

	for (int i = 0; i < np; i++)
	{
		// Reverse the order of the bits in num - e.g. 1101000 becomes 0001011:
		// This turns the face order from 0,1,2,3,4,5,6,7 to like 0,4,2,6,1,5,7,3 which gives better output in CSG operations.
		// For example, if we're subtracting a cylinder from a square, we'll first sweep off the big 90deg chunks (0,4,2,6)
		// then the 45deg ones (1,5,7,3). 
		int j = 0;
		for (int k = 0; k < hb; k++)
		{
			if (i & (1 << k))
			{
				j |= 1 << (hb - k - 1);
			}
		}

		if (j < segments)
		{
			float yaw = (float)j / float(segments) * 2 * GLM_PIf;
			brush->planes.push_back({ Plane(float3( cos(yaw), sin(yaw), 0), radius) });
		}
	}

	CSGBuildBrush(brush);
	return brush;
}

CSGBrush* CSGCreateCube(float3 size)
{
	CSGBrush* brush = CSGCreateBrush();
	brush->planes.push_back({ Plane(float3(0, 0,  1), size.z) });
	brush->planes.push_back({ Plane(float3(0, 0,  -1), size.z) });
	brush->planes.push_back({ Plane(float3( 1, 0, 0), size.x) });
	brush->planes.push_back({ Plane(float3( -1, 0, 0), size.x) });
	brush->planes.push_back({ Plane(float3(0,  1, 0), size.y) });
	brush->planes.push_back({ Plane(float3(0,  -1, 0), size.y) });
	CSGBuildBrush(brush);
	return brush;
}

void CSGBrushTransform(CSGBrush* brush, const float4x4& transform)
{
	for (CSGPlane& plane : brush->planes)
	{
		plane.plane = plane.plane * transform;
	}
	CSGBuildBrush(brush);
}