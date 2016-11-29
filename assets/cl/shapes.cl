#ifndef __SHAPES_CL
#define __SHAPES_CL
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

	float u_size, v_size;
} Plane;

typedef enum
{
	SphereType,
	PlaneType,
	MeshType
} ShapeType;

bool intersectRaySphere(const Ray* ray, const Sphere* sphere, float* time)
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

#define EPSILON 0.0000001

bool intersectRayTriangle(const Ray* ray, const float3* vertices, float* time) {
	float3 edge1 = vertices[1] - vertices[0];
	float3 edge2 = vertices[2] - vertices[0];
	float3 P = cross(ray->direction, edge2);
	float det = dot(edge1, P);
	if (det > -EPSILON && det < EPSILON) return false;
	float inv_det = 1.f/ det;
	float3 T = ray->origin - vertices[0];
	float u = dot(T,P) * inv_det;
	if (u < 0.f || u > 1.f) return false;
	float3 Q = cross(T,edge1);
	float v = dot(ray->direction,Q) * inv_det;
	if (v < 0.f || v > 1.f) return false;
	float t = dot(edge2, Q) * inv_det;
	if (t > EPSILON) {
		*time = t;
		return true;
	}
	return false;
}

bool intersectRayPlane(const Ray* ray, const Plane* plane, float* time)
{
	// http://stackoverflow.com/questions/23975555/how-to-do-ray-plane-intersection
	float denom = dot(plane->normal, ray->direction);
    if (fabs(denom) > 1e-6f)// Check that ray not parallel to plane
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

bool intersectLineSphere(const Line* line, const Sphere* sphere, float* time)
{
	float t;
	float3 dir = line->dest - line->origin;
	float maxT2 = dot(dir, dir);

	Ray ray;
	ray.origin = line->origin;
	ray.direction = normalize(dir);
	if (intersectRaySphere(&ray, sphere, &t) && (t*t) < maxT2)
	{
		*time = t;
		return true;
	}
	return false;
}

bool intersectLinePlane(const Line* line, const Plane* plane, float* time)
{
	float t;
	float3 dir = line->dest - line->origin;
	float maxT2 = dot(dir, dir);

	Ray ray;
	ray.origin = line->origin;
	ray.direction = normalize(dir);
	if (intersectRayPlane(&ray, plane, &t) && (t*t) < maxT2)
	{
		*time = t;
		return true;
	}
	return false;
}

float intersectInsideSphere(const Ray* ray, const Sphere* sphere, float3* outNormal)
{
	float3 distance = sphere->centre - ray->origin;

	// calculates the projection of the distance on the ray
	float component_parallel = dot(distance, ray->direction);
	float3 vec_parallel = component_parallel * ray->direction;

	// calculates the normal component to compare it with the radius
	float3 vec_normal = distance - vec_parallel;
	float component_normal_squared = dot(vec_normal, vec_normal);
	float radius_squared = sphere->radius * sphere->radius;

	// the time is the "t" parameter in the P = O + rD equation of the ray
	// we use pythagoras's theorem to calc the missing cathetus
	float t = component_parallel + sqrt(radius_squared - component_normal_squared);
	*outNormal = normalize(sphere->centre - (ray->origin + ray->direction * t));
	return t;

	/*if (component_normal_squared < radius_squared) {
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
	return false;*/
}

float intersectInsidePlane(const Ray* ray, const Plane* plane, float3* outNormal)
{
	*outNormal = (float3)(0.0f, 0.0f, 0.0f);
	return 0.0f;// Cant be inside a 2D plane
}

#endif// __SHAPES_CL