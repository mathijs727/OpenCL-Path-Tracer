
#ifndef __SCENE_CL
#define __SCENE_CL
#include "shapes.cl"
#include "rawshapes.cl"
#include "ray.cl"

typedef struct
{
	int numSpheres, numPlanes;
	Sphere spheres[128];
	Plane planes[128];
	Material sphereMaterials[128];
	Material planeMaterials[128];
} Scene;

typedef enum
{
	SphereType,
	PlaneType
} IntersectionType;

void loadScene(
	int numSpheres,
	__global RawSphere* spheres,
	int numPlanes,
	__global RawPlane* planes,
	__global RawMaterial* materials,
	__local Scene* scene) {
	
	scene->numSpheres = numSpheres;
	scene->numPlanes = numPlanes;
	for (int i = 0; i < numSpheres; i++)
	{
		RawSphere rawSphere = spheres[i];
		Sphere sphere;
		convertRawSphere(&rawSphere, &sphere);
		scene->spheres[i] = sphere;

		RawMaterial rawMaterial = materials[i];
		Material material;
		convertRawMaterial(&rawMaterial, &material);
		scene->sphereMaterials[i] = material;
	}

	for (int i = 0; i < numPlanes; i++)
	{
		RawPlane rawPlane = planes[i];
		Plane plane;
		convertRawPlane(&rawPlane, &plane);
		scene->planes[i] = plane;

		RawMaterial rawMaterial = materials[i + numSpheres];
		Material material;
		convertRawMaterial(&rawMaterial, &material);
		scene->planeMaterials[i] = material;
	}


}

float3 traceRay(const __local Scene* scene, const Ray* ray)
{
	float minT = 100000.0f;
	int i_current_hit = -1;
	IntersectionType type;
	// Check sphere intersections
	for (int i = 0; i < scene->numSpheres; i++)
	{
		float t;
		Sphere sphere = scene->spheres[i];
		if (intersectSphere(ray, &sphere, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = SphereType;
		}
	}

	for (int i = 0; i < scene->numPlanes; i++)
	{
		float t;
		Plane plane = scene->planes[i];
		if (intersectPlane(ray, &plane, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = PlaneType;
		}
	}

	Material material;
	if (type == SphereType)
	{
		material = scene->sphereMaterials[i_current_hit];
	} else if (type == PlaneType)
	{
		material = scene->planeMaterials[i_current_hit];
	}
	return material.colour;
}

#endif// __SCENE_CL