#include "ray.cl"

typedef struct
{
	float3 centre;
	float radius;
} Sphere;

typedef struct
{
	float3 normal;
	float offset;
} Plane;

bool intersectSphere(const Ray* ray, const Sphere* sphere, float* time)
{
	float3 distance = sphere->centre - ray->origin;

	// calculates the projection of the distance on the ray
	float component_parallel = dot(distance, ray->direction);
	float3 vec_parallel = component_parallel * ray->direction;

	// calculates the normal component to compare it with the radius
	float3 vec_normal = distance - vec_parallel;
	float component_normal_squared = dot(vec_normal, vec_normal);
	float radius_squared = sphere->radius * sphere->radius;

	if (component_normal_squared < radius_squared) {
		// the time is the "t" parameter in the P = O + rD equation of the ray
		// we use pythagoras's theorem to calc the missing cathetus
		float t = component_parallel - sqrt(radius_squared - component_normal_squared);
		if (t > 0.0f)
		{
			*time = t;
			return true;
		}
		else {
			return false;
		}
	}
	return false;
}

bool intersectPlane(const Ray* ray, const Plane* plane, float* time)
{
	// http://stackoverflow.com/questions/23975555/how-to-do-ray-plane-intersection
	float denom = dot(plane->normal, ray->direction);
    if (fabs(denom) > 1e-6)// Check that ray not parallel to plane
	{
		// A known point on the plane
		float3 center = plane->offset * plane->normal;
		float t = dot(center - ray->origin, plane->normal) / denom;
		if (t > 0.0f)
		{
			*time = t;
			return true;
		}
	}
	return false;
}

