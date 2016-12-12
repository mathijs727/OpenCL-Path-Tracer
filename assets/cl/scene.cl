#ifndef __SCENE_CL
#define __SCENE_CL
#include "shapes.cl"
#include "ray.cl"
#include "light.cl"
#include "stack.cl"
#include "bvh.cl"

#define USE_BVH_PRIMARY
// Only test with primary rays for now
#define USE_BVH_LIGHT

// At least on AMD, this is not defined
#define NULL 0

typedef struct
{
	int numLights, numVertices, numTriangles;
	const __global VertexData* vertices;
	const __global TriangleData* triangles;
	const __global Material* meshMaterials;
	const __global Light* lights;

	const __global ThinBvhNode* thinBvh;
	const __global FatBvhNode* topLevelBvh;
	int topLevelBvhRoot;

	float refractiveIndex;
} Scene;

void getVertices(VertexData* out_vertices, uint* indices, const Scene* scene) {
	out_vertices[0] = scene->vertices[indices[0]];
	out_vertices[1] = scene->vertices[indices[1]];
	out_vertices[2] = scene->vertices[indices[2]];
}

void loadScene(
	int numVertices,
	const __global VertexData* vertices,
	int numTriangles,
	const __global TriangleData* triangles,
	const __global Material* materials,
	int numLights,
	const __global Light* lights,
	const __global ThinBvhNode* thinBvh,
	int topLevelBvhRoot,
	const __global FatBvhNode* topLevelBvh,
	Scene* scene) {
	scene->refractiveIndex =  1.000277f;
	
	scene->numLights = numLights;
	scene->numVertices = numVertices;
	scene->numTriangles = numTriangles;

	scene->meshMaterials = materials;
	scene->triangles = triangles;
	scene->vertices = vertices;
	scene->lights = lights;

	scene->thinBvh = thinBvh;
	scene->topLevelBvh = topLevelBvh;
	scene->topLevelBvhRoot = topLevelBvhRoot;
}

bool traceRay(
	const Scene* scene,
	const Ray* ray,
	bool hitAny,
	float maxT,
	int* outTriangleIndex,
	float* outT,
	float2* outUV)
{
	int triangleIndex;
	float closestT = maxT;
	float2 closestUV;

	int boxIntersectionTests = 0;
	int triangleIntersectionTests = 0;
	
#ifdef USE_BVH_PRIMARY
	// Check mesh intersection using BVH traversal
	ThinBvhNode thinBvhStack[30];
	int thinBvhStackPtr = 0;

	// Traverse top level BVH and add relevant sub-BVH's to the "thin BVH" stacks
	unsigned int topLevelBvhStack[10];
	unsigned int topLevelBvhStackPtr = 0;
	topLevelBvhStack[topLevelBvhStackPtr++] = scene->topLevelBvhRoot;
	while (topLevelBvhStackPtr > 0)
	{
		FatBvhNode node = scene->topLevelBvh[topLevelBvhStack[--topLevelBvhStackPtr]];
		if (!intersectRayFatBvh(ray, &node))
			continue;

		if (node.isLeaf)
		{
			thinBvhStack[thinBvhStackPtr++] = scene->thinBvh[node.thinBvh];
		} else {
			topLevelBvhStack[topLevelBvhStackPtr++] = node.leftChildIndex;
			topLevelBvhStack[topLevelBvhStackPtr++] = node.rightChildIndex;
		}
	}


	while (thinBvhStackPtr > 0)
	{
		boxIntersectionTests++;
		ThinBvhNode node = thinBvhStack[--thinBvhStackPtr];

		if (!intersectRayThinBvh(ray, &node, closestT))
			continue;

		if (node.triangleCount != 0)// isLeaf()
		{
			for (int i = 0; i < node.triangleCount; i++)
			{
				triangleIntersectionTests++;

				float t;
				float2 uv;

				VertexData vertices[3];
				TriangleData triangle = scene->triangles[node.firstTriangleIndex + i];
				getVertices(vertices, triangle.indices, scene);
				if (intersectRayTriangle(ray, vertices, &t, &uv) && t < closestT)
				{
					if (hitAny)
						return true;

					triangleIndex = node.firstTriangleIndex + i;
					closestT = t;
					closestUV = uv;
				}
			}
		} else {
			// Ordered traversal
			ThinBvhNode left, right;
			left = scene->thinBvh[node.leftChildIndex + 0];
			right = scene->thinBvh[node.leftChildIndex + 1];

			float3 leftVec = left.centre - ray->origin;
			float3 rightVec = right.centre - ray->origin;

			if (dot(leftVec, leftVec) < dot(rightVec, rightVec))
			{
				thinBvhStack[thinBvhStackPtr++] = right;
				thinBvhStack[thinBvhStackPtr++] = left;
			} else {
				thinBvhStack[thinBvhStackPtr++] = right;
				thinBvhStack[thinBvhStackPtr++] = left;
			}
		}
	}

	if (closestT != maxT)// We did not hit anything
	{
		*outTriangleIndex = triangleIndex;
		*outT = closestT;
		*outUV = closestUV;
		return true;
	} else {// We did not intersect with any triangle
		return false;
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
		if (!backface && intersectRayTriangle(ray, &outT, &outUv) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = MeshType;
			normal = n;
			tex_coords = tex_coords_tmp;
		}
	}
#endif
}

#endif// __SCENE_CL