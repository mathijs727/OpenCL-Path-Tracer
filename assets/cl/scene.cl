#ifndef __SCENE_CL
#define __SCENE_CL
#include "shapes.cl"
#include "ray.cl"
#include "light.cl"
#include "stack.cl"
#include "bvh.cl"
#include "math.cl"

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
	const __global float* invTransform;
} ThinBvhStackItem;

bool traceRay(
	const Scene* scene,
	const Ray* ray,
	bool hitAny,
	float maxT,
	int* outTriangleIndex,
	float* outT,
	float2* outUV,
#ifdef COUNT_TRAVERSAL
	__global const float** outInvTransform,
	int* count)
#else
	__global const float** outInvTransform)
#endif
{
	int triangleIndex;
	float closestT = maxT;
	float2 closestUV;
	const __global float* closestMatrix;

#ifdef COUNT_TRAVERSAL
	if (count) *count = 0;
#endif
	// Check mesh intersection using BVH traversal
	ThinBvhStackItem thinBvhStack[128];
	int thinBvhStackPtr = 0;

	// Traverse top level BVH and add relevant sub-BVH's to the "thin BVH" stacks
	unsigned int topLevelBvhStack[16];
	unsigned int topLevelBvhStackPtr = 0;
	topLevelBvhStack[topLevelBvhStackPtr++] = scene->topLevelBvhRoot;

	while (true)
	{
		if (topLevelBvhStackPtr == 0)
			break;

		while (topLevelBvhStackPtr > 0)
		{
#ifdef COUNT_TRAVERSAL
			if (count) *count = *count + 1;
#endif
			const __global FatBvhNode* node = &scene->topLevelBvh[topLevelBvhStack[--topLevelBvhStackPtr]];
			if (!intersectRayFatBvh(ray, node, closestT))
				continue;

			if (node->isLeaf)
			{
				/*Ray transformedRay;
				transformedRay.origin = matrixMultiply(node.invTransform, (float4)(ray->origin, 1.0f)).xyz;
				transformedRay.direction = normalize(matrixMultiply(node.invTransform, (float4)(ray->direction, 0.0f)).xyz);*/

				thinBvhStack[thinBvhStackPtr].nodeIndex = node->thinBvh;
				thinBvhStack[thinBvhStackPtr].invTransform = node->invTransform;
				thinBvhStackPtr++;
				break;// Stop top lvl traversal and check out this thin bvh
			} else {
				// Calculate which childs' AABB centre is closer to the ray's origin
				float3 leftMin = scene->topLevelBvh[node->leftChildIndex].min;
				float3 leftMax = scene->topLevelBvh[node->leftChildIndex].max;
				float3 rightMin = scene->topLevelBvh[node->rightChildIndex].min;
				float3 rightMax = scene->topLevelBvh[node->rightChildIndex].max;

				float3 leftVec = (leftMin + leftMax) / 2.0f - ray->origin;
				float3 rightVec = (rightMin + rightMax) / 2.0f - ray->origin;

				if (dot(leftVec, leftVec) < dot(rightVec, rightVec))
				{
					topLevelBvhStack[topLevelBvhStackPtr++] = node->rightChildIndex;
					topLevelBvhStack[topLevelBvhStackPtr++] = node->leftChildIndex;
				} else {
					topLevelBvhStack[topLevelBvhStackPtr++] = node->leftChildIndex;
					topLevelBvhStack[topLevelBvhStackPtr++] = node->rightChildIndex;
				}
			}
		}

		while (thinBvhStackPtr > 0)
		{
			ThinBvhStackItem item = thinBvhStack[--thinBvhStackPtr];
			ThinBvhNode node = scene->thinBvh[item.nodeIndex];

			Ray transformedRay;
			transformedRay.origin = matrixMultiply(item.invTransform, (float4)(ray->origin, 1.0f)).xyz;
			transformedRay.direction = normalize(matrixMultiply(item.invTransform, (float4)(ray->direction, 0.0f)).xyz);

#ifdef COUNT_TRAVERSAL
			if (count) *count = *count + 1;
#endif

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
						closestMatrix = item.invTransform;
					}
				}
			} else {
				// Ordered traversal
				ThinBvhNode left, right;
				left = scene->thinBvh[node.leftChildIndex + 0];
				right = scene->thinBvh[node.leftChildIndex + 1];

				float3 leftVec = (left.min + left.max) / 2.0f - transformedRay.origin;
				float3 rightVec = (right.min + right.max) / 2.0f - transformedRay.origin;

				if (dot(leftVec, leftVec) < dot(rightVec, rightVec))
				{
					// Right child;
					thinBvhStack[thinBvhStackPtr].nodeIndex = node.leftChildIndex + 1;
					thinBvhStack[thinBvhStackPtr].invTransform = item.invTransform;
					thinBvhStackPtr++;

					// Left child
					thinBvhStack[thinBvhStackPtr].nodeIndex = node.leftChildIndex + 0;
					thinBvhStack[thinBvhStackPtr].invTransform = item.invTransform;
					thinBvhStackPtr++;
				} else {
					// Left child
					thinBvhStack[thinBvhStackPtr].nodeIndex = node.leftChildIndex + 0;
					thinBvhStack[thinBvhStackPtr].invTransform = item.invTransform;
					thinBvhStackPtr++;

					// Right child;
					thinBvhStack[thinBvhStackPtr].nodeIndex = node.leftChildIndex + 1;
					thinBvhStack[thinBvhStackPtr].invTransform = item.invTransform;
					thinBvhStackPtr++;
				}// Ordered of traversal
			}// If/else node contains triangles
		}// Thin bvh traversal
	}// Traversal

	if (closestT != maxT)// We did not hit anything
	{
		*outTriangleIndex = triangleIndex;
		*outT = closestT;
		*outUV = closestUV;
		*outInvTransform = closestMatrix;
		return true;
	} else {// We did not intersect with any triangle
		return false;
	}
}

#endif// __SCENE_CL