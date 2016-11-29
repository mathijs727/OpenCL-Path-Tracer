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

#define EPSILON 0.00001

// as described in https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
bool intersectRayTriangle(const Ray* ray, const float3* vertices, float* time) {
	float3 O = ray->origin;
	float3 D = ray->direction;
	float3 V1 = vertices[0];
	float3 V2 = vertices[1];
	float3 V3 = vertices[2];
	float3 e1, e2;  //Edge1, Edge2
	float3 P, Q, T;
	float det, inv_det, u, v;
	float t;

	//Find vectors for two edges sharing V1
	e1 = V2 - V1;
	e2 = V3 - V1;
	//Begin calculating determinant - also used to calculate u parameter
	P = cross(D,e2);
	//if determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
	det = dot(e1, P);
	//NOT CULLING
	if(det > -EPSILON && det < EPSILON) return 0;
	inv_det = 1.f / det;

	//calculate distance from V1 to ray origin
	T = O - V1;

	//Calculate u parameter and test bound
	u = dot(T, P) * inv_det;
	//The intersection lies outside of the triangle
	if(u < 0.f || u > 1.f) return 0;

	//Prepare to test v parameter
	Q = cross(T, e1);

	//Calculate V parameter and test bound
	v = dot(D, Q) * inv_det;
	//The intersection lies outside of the triangle
	if(v < 0.f || u + v  > 1.f) return 0;

	t = dot(e2, Q) * inv_det;

	if(t > EPSILON) { //ray intersection
		*time = t;
		return 1;
	}

	// No hit, no win
	return 0;
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