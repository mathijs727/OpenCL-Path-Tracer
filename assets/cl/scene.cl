#ifndef __SCENE_CL
#define __SCENE_CL
#include "shapes.cl"
#include "rawshapes.cl"
#include "ray.cl"
#include "light.cl"
#include "stack.cl"

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

	float refractiveIndex;
} Scene;


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

	scene->numLights = numLights;
	for (int i = 0; i < numLights; i++)
	{
		RawLight rawLight = lights[i];
		Light light;
		convertRawLight(&rawLight, &light);
		scene->lights[i] = light;
	}

	scene->refractiveIndex =  1.000277f;
}

bool checkRay(const __local Scene* scene, const Ray* ray)
{
	// Check sphere intersections
	for (int i = 0; i < scene->numSpheres; i++)
	{
		float t;
		Sphere sphere = scene->spheres[i];
		if (intersectRaySphere(ray, &sphere, &t))
		{
			return true;
		}
	}

	// Check plane intersection
	for (int i = 0; i < scene->numPlanes; i++)
	{
		float t;
		Plane plane = scene->planes[i];
		if (intersectRayPlane(ray, &plane, &t))
		{
			return true;
		}
	}

	return false;
}

bool checkLine(const __local Scene* scene, const Line* line)
{
	// Check sphere intersections
	for (int i = 0; i < scene->numSpheres; i++)
	{
		float t;
		Sphere sphere = scene->spheres[i];
		if (intersectLineSphere(line, &sphere, &t))
		{
			return true;
		}
	}

	// Check plane intersection
	for (int i = 0; i < scene->numPlanes; i++)
	{
		float t;
		Plane plane = scene->planes[i];
		if (intersectLinePlane(line, &plane, &t))
		{
			return true;
		}
	}

	return false;
}

#include "shading.cl"
float3 traceRay(
	const __local Scene* scene,
	const Ray* ray,
	float3 multiplier,
	Stack* stack)
{
	float minT = 100000.0f;
	int i_current_hit = -1;
	ShapeType type;
	void* shape;
	// Storage for the shape pointer that only expires at the end of this function
	Plane plane;
	Sphere sphere;

	// Check sphere intersections
	for (int i = 0; i < scene->numSpheres; i++)
	{
		float t;
		sphere = scene->spheres[i];
		if (intersectRaySphere(ray, &sphere, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = SphereType;
			shape = (void*)&sphere;
		}
	}

	// Check plane intersection
	for (int i = 0; i < scene->numPlanes; i++)
	{
		float t;
		plane = scene->planes[i];
		if (intersectRayPlane(ray, &plane, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = PlaneType;
			shape = (void*)&plane;
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
			scene,
			ray->direction,
			intersection,
			normal,
			type,
			shape,
			&material,
			multiplier,
			stack);
	}
	return (float3)(0.0f, 0.0f, 0.0f);
}

#endif// __SCENE_CL