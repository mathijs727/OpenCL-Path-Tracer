#ifndef __SCENE_CL
#define __SCENE_CL
#include "shapes.cl"
#include "ray.cl"
#include "light.cl"
#include "stack.cl"
#include "bvh.cl"
#include "math.cl"

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

typedef struct
{
	unsigned int nodeIndex;
	Ray transformedRay;
} ThinBvhStackItem;

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

#ifdef USE_BVH_PRIMARY
	// Check mesh intersection using BVH traversal
	ThinBvhStackItem thinBvhStack[48];
	int thinBvhStackPtr = 0;

	// Traverse top level BVH and add relevant sub-BVH's to the "thin BVH" stacks
	unsigned int topLevelBvhStack[16];
	unsigned int topLevelBvhStackPtr = 0;
	topLevelBvhStack[topLevelBvhStackPtr++] = scene->topLevelBvhRoot;
	while (topLevelBvhStackPtr > 0)
	{
		unsigned int index = topLevelBvhStack[--topLevelBvhStackPtr];
		FatBvhNode node = scene->topLevelBvh[index];
		if (!intersectRayFatBvh(ray, &node))
			continue;

		if (node.isLeaf)
		{
			Ray transformedRay;
			transformedRay.origin = matrixMultiply(node.invTransform, (float4)(ray->origin, 1.0f)).xyz;
			transformedRay.direction = normalize(matrixMultiply(node.invTransform, (float4)(ray->direction, 0.0f)).xyz);

			thinBvhStack[thinBvhStackPtr].nodeIndex = node.thinBvh;
			thinBvhStack[thinBvhStackPtr].transformedRay = transformedRay;
			thinBvhStackPtr++;
		} else {
			topLevelBvhStack[topLevelBvhStackPtr++] = node.leftChildIndex;
			topLevelBvhStack[topLevelBvhStackPtr++] = node.rightChildIndex;
		}
	}


	while (thinBvhStackPtr > 0)
	{
		ThinBvhStackItem* item = &thinBvhStack[--thinBvhStackPtr];
		ThinBvhNode node = scene->thinBvh[item->nodeIndex];
		Ray transformedRay = item->transformedRay;

		if (!intersectRayThinBvh(&transformedRay, &node, closestT))
			continue;

		if (node.triangleCount != 0)// isLeaf()
		{
			for (int i = 0; i < node.triangleCount; i++)
			{
				float t;
				float2 uv;

				VertexData vertices[3];
				TriangleData triangle = scene->triangles[node.firstTriangleIndex + i];
				getVertices(vertices, triangle.indices, scene);
				if (intersectRayTriangle(&transformedRay, vertices, &t, &uv) && t < closestT)
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

			float3 leftVec = left.centre - transformedRay.origin;
			float3 rightVec = right.centre - transformedRay.origin;

			if (dot(leftVec, leftVec) < dot(rightVec, rightVec))
			{
				// Right child;
				thinBvhStack[thinBvhStackPtr].nodeIndex = node.leftChildIndex + 1;
				thinBvhStack[thinBvhStackPtr].transformedRay = transformedRay;
				thinBvhStackPtr++;

				// Left child
				thinBvhStack[thinBvhStackPtr].nodeIndex = node.leftChildIndex + 0;
				thinBvhStack[thinBvhStackPtr].transformedRay = transformedRay;
				thinBvhStackPtr++;
			} else {
				// Left child
				thinBvhStack[thinBvhStackPtr].nodeIndex = node.leftChildIndex + 0;
				thinBvhStack[thinBvhStackPtr].transformedRay = transformedRay;
				thinBvhStackPtr++;

				// Right child;
				thinBvhStack[thinBvhStackPtr].nodeIndex = node.leftChildIndex + 1;
				thinBvhStack[thinBvhStackPtr].transformedRay = transformedRay;
				thinBvhStackPtr++;
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