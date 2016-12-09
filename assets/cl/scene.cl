#ifndef __SCENE_CL
#define __SCENE_CL
#include "shapes.cl"
#include "rawshapes.cl"
#include "ray.cl"
#include "light.cl"
#include "stack.cl"
#include "bvh.cl"

#define USE_BVH_PRIMARY
// Only test with primary rays for now
#define USE_BVH_LIGHT

typedef struct
{
	int numSpheres, numPlanes, numLights, numVertices, numTriangles;
	const __global Sphere* spheres;
	const __global Plane* planes;
	const __global VertexData* vertices;
	const __global TriangleData* triangles;
	const __global Material* sphereMaterials;
	const __global Material* planeMaterials;
	const __global Material* meshMaterials;
	const __global Light* lights;

	const __global ThinBvhNodeSerialized* thinBvh;

	float refractiveIndex;
} Scene;

void getVertices(VertexData* out_vertices, uint* indices, const Scene* scene) {
	out_vertices[0] = scene->vertices[indices[0]];
	out_vertices[1] = scene->vertices[indices[1]];
	out_vertices[2] = scene->vertices[indices[2]];
}

void loadScene(
	int numSpheres,
	const __global Sphere* spheres,
	int numPlanes,
	const __global Plane* planes,
	int numVertices,
	const __global VertexData* vertices,
	int numTriangles,
	const __global TriangleData* triangles,
	const __global Material* materials,
	int numLights,
	const __global Light* lights,
	const __global ThinBvhNodeSerialized* thinBvh,
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

	scene->thinBvh = thinBvh;
}

bool checkRay(const Scene* scene, const Ray* ray)
{
	float3 n;
	float2 texCoords;

	// Check sphere intersections
	for (int i = 0; i < scene->numSpheres; i++)
	{
		float t;
		Sphere sphere = scene->spheres[i];
		if (intersectRaySphere(ray, &sphere, &n, &texCoords, &t))
		{
			return true;
		}
	}

	// Check plane intersection
	for (int i = 0; i < scene->numPlanes; i++)
	{
		float t;
		Plane plane = scene->planes[i];
		if (intersectRayPlane(ray, &plane, &n, &texCoords, &t))
		{
			return true;
		}
	}

#ifdef USE_BVH_LIGHT
	// check mesh intersection using BVH traversal
	ThinBvhNode bvhStack[50];
	loadThinBvhNode(&scene->thinBvh[0], &bvhStack[0]);
	int bvhStackPtr = 1;

	while (bvhStackPtr > 0)
	{
		ThinBvhNode node = bvhStack[--bvhStackPtr];

		if (!intersectRayThinBvh(ray, &node, INFINITY))
			continue;

		if (node.triangleCount != 0)// isLeaf()
		{
			for (int i = 0; i < node.triangleCount; i++)
			{
				float t;
				float3 n;
				float2 tex_coords_tmp;

				VertexData v[3];
				TriangleData triangle = scene->triangles[node.firstTriangleIndex + i];
				getVertices(v, triangle.indices, scene);
				if (intersectRayTriangle(ray, v, &n, &tex_coords_tmp, &t))
				{
					return true;
				}
			}
		} else {
			// Ordered traversal
			ThinBvhNode left, right;
			loadThinBvhNode(
				&scene->thinBvh[node.leftChildIndex + 0],
				&left);
			loadThinBvhNode(
				&scene->thinBvh[node.leftChildIndex + 1],
				&right);

			float3 leftVec = (left.min + left.max) / 2.0f - ray->origin;
			float3 rightVec = (right.min + right.max) / 2.0f - ray->origin;

			if (dot(leftVec, leftVec) < dot(rightVec, rightVec))
			{
				bvhStack[bvhStackPtr++] = right;
				bvhStack[bvhStackPtr++] = left;
			} else {
				bvhStack[bvhStackPtr++] = right;
				bvhStack[bvhStackPtr++] = left;
			}
		}
	}
#else
	// check triangles
	for (int i = 0; i < scene->numTriangles; i++)
	{
		float t;
		TriangleData triang = scene->triangles[i];
		VertexData vertices[3];
		getVertices(vertices, triang.indices, scene);
		if (intersectRayTriangle(ray, vertices, &n, &texCoords, &t))
		{
			return true;
		}
	}
#endif

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

#ifdef USE_BVH_LIGHT
	// check mesh intersection using BVH traversal
	ThinBvhNode bvhStack[20];
	loadThinBvhNode(&scene->thinBvh[0], &bvhStack[0]);
	int bvhStackPtr = 1;

	while (bvhStackPtr > 0)
	{
		ThinBvhNode node = bvhStack[--bvhStackPtr];

		if (!intersectLineThinBvh(line, &node))
			continue;

		if (node.triangleCount != 0)// isLeaf()
		{
			for (int i = 0; i < node.triangleCount; i++)
			{
				float t;
				VertexData vertices[3];
				TriangleData triangle = scene->triangles[node.firstTriangleIndex + i];
				getVertices(vertices, triangle.indices, scene);
				if (intersectLineTriangle(line, vertices, &t))
				{
					return true;
				}
			}
		} else {
			// Ordered traversal
			ThinBvhNode left, right;
			loadThinBvhNode(
				&scene->thinBvh[node.leftChildIndex + 0],
				&left);
			loadThinBvhNode(
				&scene->thinBvh[node.leftChildIndex + 1],
				&right);

			float3 leftVec = (left.min + left.max) / 2.0f - line->origin;
			float3 rightVec = (right.min + right.max) / 2.0f - line->origin;

			if (dot(leftVec, leftVec) < dot(rightVec, rightVec))
			{
				bvhStack[bvhStackPtr++] = right;
				bvhStack[bvhStackPtr++] = left;
			} else {
				bvhStack[bvhStackPtr++] = right;
				bvhStack[bvhStackPtr++] = left;
			}
		}
	}
#else
	// check triangles
	for (int i = 0; i < scene->numTriangles; i++)
	{
		float t;
		TriangleData triang = scene->triangles[i];
		VertexData vertices[3];
		getVertices(vertices, triang.indices, scene);
		if (intersectLineTriangle(line, vertices, &t))
		{
			return true;
		}
	}
#endif

	return false;
}

#include "shading.cl"
float3 traceRay(
	const Scene* scene,
	const Ray* ray,
	float3 multiplier,
	Stack* stack,
	image2d_array_t textures)
{
	float minT = 100000.0f;
	int i_current_hit = -1;
	ShapeType type;
	float3 normal;
	float2 tex_coords;
	
	// Check sphere intersections
	for (int i = 0; i < scene->numSpheres; i++)
	{
		float t;
		float3 n;
		float2 tex_coords_tmp;
		Sphere s = scene->spheres[i];
		if (intersectRaySphere(ray, &s, &n, &tex_coords_tmp, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = SphereType;
			normal = n;
			tex_coords = tex_coords_tmp;
		}
	}

	// Check plane intersection
	for (int i = 0; i < scene->numPlanes; i++)
	{
		float t;
		float3 n;
		float2 tex_coords_tmp;
		Plane p = scene->planes[i];
		if (intersectRayPlane(ray, &p, &n, &tex_coords_tmp, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = PlaneType;
			normal = n;
			tex_coords = tex_coords_tmp;
		}
	}

	int boxIntersectionTests = 0;
	int triangleIntersectionTests = 0;
	
#ifdef USE_BVH_PRIMARY
	// check mesh intersection using BVH traversal
	ThinBvhNode bvhStack[20];
	loadThinBvhNode(&scene->thinBvh[0], &bvhStack[0]);
	int bvhStackPtr = 1;


	while (bvhStackPtr > 0)
	{
		boxIntersectionTests++;
		ThinBvhNode node = bvhStack[--bvhStackPtr];

		if (!intersectRayThinBvh(ray, &node, minT))
			continue;

		if (node.triangleCount != 0)// isLeaf()
		{
			for (int i = 0; i < node.triangleCount; i++)
			{
				triangleIntersectionTests++;

				float t;
				float3 n;
				float2 tex_coords_tmp;

				VertexData v[3];
				TriangleData triangle = scene->triangles[node.firstTriangleIndex + i];
				getVertices(v, triangle.indices, scene);
				if (intersectRayTriangle(ray, v, &n, &tex_coords_tmp, &t) && t < minT)
				{
					minT = t;
					i_current_hit = node.firstTriangleIndex + i;
					type = MeshType;
					normal = n;
					tex_coords = tex_coords_tmp;
				}
			}
		} else {
			// Ordered traversal
			ThinBvhNode left, right;
			loadThinBvhNode(
				&scene->thinBvh[node.leftChildIndex + 0],
				&left);
			loadThinBvhNode(
				&scene->thinBvh[node.leftChildIndex + 1],
				&right);

			float3 leftVec = (left.min + left.max) / 2.0f - ray->origin;
			float3 rightVec = (right.min + right.max) / 2.0f - ray->origin;

			if (dot(leftVec, leftVec) < dot(rightVec, rightVec))
			{
				bvhStack[bvhStackPtr++] = right;
				bvhStack[bvhStackPtr++] = left;
			} else {
				bvhStack[bvhStackPtr++] = right;
				bvhStack[bvhStackPtr++] = left;
			}
		}
	}
#else
	// Old fashioned brute force
	for (int i = 0; i < scene->numTriangles; ++i) {
		float t;
		float3 n;
		float2 tex_coords_tmp;

		VertexData v[3];
		TriangleData triangle = scene->triangles[i];
		getVertices(v, triangle.indices, scene);
		float3 edge1 = v[1].vertex - v[0].vertex;
		float3 edge2 = v[2].vertex - v[0].vertex;
		bool backface = dot(ray->direction, cross(edge2, edge1)) < 0;
		if (!backface && intersectRayTriangle(ray, v, &n, &tex_coords_tmp, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = MeshType;
			normal = n;
			tex_coords = tex_coords_tmp;
		}
	}
#endif

	float3 outCol = (float3)(0.0f, 0.0f, 0.0f);
	if (i_current_hit >= 0)
	{
		// Calculate the normal of the hit surface and retrieve the material
		float3 direction = ray->direction;
		float3 intersection = minT * direction + ray->origin;
		Material material;
		if (type == SphereType) {
			material = scene->sphereMaterials[i_current_hit];
		} else if (type == PlaneType) {
			material = scene->planeMaterials[i_current_hit];
		} else if (type == MeshType) {
			material = scene->meshMaterials[scene->triangles[i_current_hit].mat_index];
		}
		//return (float3)(1.0f, 1.0f, 1.0f);
		outCol = whittedShading(
			scene,
			ray->direction,
			intersection,
			normal,
			tex_coords,
			type,
			i_current_hit,
			&material,
			textures,
			multiplier,
			stack);
	}
	int tests = boxIntersectionTests;// + triangleIntersectionTests;
	outCol.y = tests / 400.0f;
	return outCol;
}

#endif// __SCENE_CL