#include <EVA/Math.hpp>
#include <math.h>

float Intersect(const Ray& ray, const Plane& plane)
{
    float denom = Dot(plane.normal, ray.direction);
    if (fabsf(denom) < 1e-6f)
	{
        return -1;
	}
    return (plane.distance - Dot(plane.normal, ray.origin)) / denom;
}