#ifndef __SCENE_CL
#define __SCENE_CL
#include "shapes.cl"
#include "rawshapes.cl"
#include "ray.cl"
#include "light.cl"
#include "stack.cl"

typedef struct
{
	uint indices[3];
	uint mat_index;
	float3 normal;
} TriangleData;

typedef struct
{
	int numSpheres, numPlanes, numLights, numVertices, numTriangles;
	__global Sphere* spheres;
	__global Plane* planes;
	__global float3* vertices;
	__global TriangleData* triangles;
	__global Material* sphereMaterials;
	__global Material* planeMaterials;
	__global Material* meshMaterials;
	//PointLight pointLights[8];
	//DirectionalLight directionalLights[8];
	__global Light* lights;

	float refractiveIndex;
} Scene;

void getVertices(float3* out_vertices, uint* indices, Scene* scene) {
	out_vertices[0] = scene->vertices[indices[0]];
	out_vertices[1] = scene->vertices[indices[1]];
	out_vertices[2] = scene->vertices[indices[2]];
}

// TODO: instead of using a for loop in the "main" thread. Let every thread copy
//  a tiny bit of data (IE workgroups of 128).
void loadScene(
	int numSpheres,
	__global Sphere* spheres,
	int numPlanes,
	__global Plane* planes,
	int numVertices,
	__global float3* vertices,
	int numTriangles,
	__global TriangleData* triangles,
	__global Material* materials,
	int numLights,
	__global Light* lights,
	Scene* scene) {
	scene->refractiveIndex =  1.000277f;
	
	scene->numSpheres = numSpheres;
	scene->numPlanes = numPlanes;
	scene->numLights = numLights;
	scene->numVertices = numVertices;
	scene->numTriangles = numTriangles;

	scene->spheres = spheres;
	scene->planes = planes;
	scene->sphereMaterials = materials;
	scene->planeMaterials = &materials[numSpheres];
	scene->meshMaterials = &materials[numSpheres + numPlanes];
	scene->triangles = triangles;
	scene->vertices = vertices;
	scene->lights = lights;
}

bool checkRay(const Scene* scene, const Ray* ray)
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

	// check triangles
	for (int i = 0; i < scene->numTriangles; i++)
	{
		float t;
		TriangleData triang = scene->triangles[i];
		float3 vertices[3];
		getVertices(vertices, triang.indices, scene);
		if (intersectRayTriangle(ray, vertices, &t))
		{
			return true;
		}
	}

	return false;
}

bool checkLine(const Scene* scene, const Line* line)
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

	// check triangles
	for (int i = 0; i < scene->numTriangles; i++)
	{
		float t;
		TriangleData triang = scene->triangles[i];
		float3 vertices[3];
		getVertices(vertices, triang.indices, scene);
		if (intersectRayTriangle(line, vertices, &t))
		{
			return true;
		}
	}

	return false;
}

#include "shading.cl"
float3 traceRay(
	const Scene* scene,
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
	TriangleData triang;
	float3 vertices[3];
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

	// check mesh intersection
	for (int i = 0; i < scene->numTriangles; ++i) {
		float t;
		triang = scene->triangles[i];
		getVertices(vertices, triang.indices, scene);
		if (intersectRayTriangle(ray, vertices, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = MeshType;
			shape = (void*)vertices;
		}
	}

	if (i_current_hit >= 0)
	{
		// Calculate the normal of the hit surface and retrieve the material
		float3 direction = ray->direction;
		float3 intersection = minT * direction + ray->origin;
		float3 normal;
		Material material;
		if (type == SphereType) {
			normal = normalize(intersection - scene->spheres[i_current_hit].centre);
			material = scene->sphereMaterials[i_current_hit];
		} else if (type == PlaneType) {
			normal = normalize(scene->planes[i_current_hit].normal);
			material = scene->planeMaterials[i_current_hit];
		} else if (type == MeshType) {
			normal = triang.normal;
			material = scene->meshMaterials[triang.mat_index];
		}
		//return (float3)(1.0f, 1.0f, 1.0f);
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