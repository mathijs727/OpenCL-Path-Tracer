#ifndef __SCENE_CL
#define __SCENE_CL
#include "shapes.cl"
#include "rawshapes.cl"
#include "ray.cl"
#include "light.cl"
#include "shading.cl"

typedef struct
{
	int numSpheres, numPlanes, numLights;
	Sphere spheres[128];
	Plane planes[128];
	Material sphereMaterials[128];
	Material planeMaterials[128];
	//PointLight pointLights[8];
	//DirectionalLight directionalLights[8];
	Light lights[16];
} Scene;

typedef enum
{
	SphereType,
	PlaneType
} IntersectionType;


// TODO: instead of using a for loop in the "main" thread. Let every thread copy
//  a tiny bit of data (IE workgroups of 128).
void loadScene(
	int numSpheres,
	__global RawSphere* spheres,
	int numPlanes,
	__global RawPlane* planes,
	__global RawMaterial* materials,
	int numLights,
	__global RawLight* lights,
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

		RawMaterial rawMaterial = materials[numSpheres + i];
		Material material;
		convertRawMaterial(&rawMaterial, &material);
		scene->planeMaterials[i] = material;
	}

	//TODO: morgen verder gaan...
	scene->numLights = numLights;
	for (int i = 0; i < numLights; i++)
	{
		RawLight rawLight = lights[i];
		Light light;
		convertRawLight(&rawLight, &light);
		scene->lights[i] = light;
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
		if (intersectRaySphere(ray, &sphere, &t) && t < minT)
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
		if (intersectRayPlane(ray, &plane, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = PlaneType;
		}
	}

	if (i_current_hit >= 0)
	{
		// Calculate the normal of the hit surface and retrieve the material
		float3 direction = ray->direction;
		float3 intersection = minT * direction + ray->origin;
		float3 normal;
		Material material;
		if (type == SphereType)
		{
			normal = normalize(intersection - scene->spheres[i_current_hit].centre);
			material = scene->sphereMaterials[i_current_hit];
		} else if (type == PlaneType)
		{
			normal = normalize(scene->planes[i_current_hit].normal);
			material = scene->planeMaterials[i_current_hit];
		}

		return whittedShading(
			&ray->direction,
			&intersection,
			&normal,
			scene->numLights,
			&scene->lights,
			&material);
	}
	return (float3)(0.0f, 0.0f, 0.0f);
}

#endif// __SCENE_CL