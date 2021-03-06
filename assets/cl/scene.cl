#ifndef __SCENE_CL
#define __SCENE_CL
#include "shapes.cl"
#include "ray.cl"
#include "light.cl"
#include "bvh.cl"
#include "math.cl"

// At least on AMD, this is not defined
#define NULL 0

typedef struct
{
	uint numVertices, numTriangles, numEmissiveTriangles, numLights;
	const __global VertexData* vertices;
	const __global TriangleData* triangles;
	const __global Material* meshMaterials;

	const __global EmissiveTriangle* emissiveTriangles;

	const __global SubBvhNode* subBvh;
	const __global TopBvhNode* topLevelBvh;
	uint topLevelBvhRoot;

	float refractiveIndex;

	int cubemapTextureIndices[6];
} Scene;

void getVertices(VertexData* out_vertices, uint* indices, const __local Scene* scene) {
	out_vertices[0] = scene->vertices[indices[0]];
	out_vertices[1] = scene->vertices[indices[1]];
	out_vertices[2] = scene->vertices[indices[2]];
}

void loadScene(
	const __global VertexData* vertices,
	const __global TriangleData* triangles,
	const __global Material* materials,
	uint numEmissiveTriangles,
	const __global EmissiveTriangle* emissiveTriangles,
	const __global SubBvhNode* subBvh,
	uint topLevelBvhRoot,
	const __global TopBvhNode* topLevelBvh,
	__local Scene* scene) {
	scene->refractiveIndex =  1.000277f;
	
	scene->numEmissiveTriangles = numEmissiveTriangles;

	scene->vertices = vertices;
	scene->triangles = triangles;
	scene->meshMaterials = materials;
	
	scene->emissiveTriangles = emissiveTriangles;
 
	scene->subBvh = subBvh;
	scene->topLevelBvh = topLevelBvh;
	scene->topLevelBvhRoot = topLevelBvhRoot;
}

bool traceRay(
	__global uint* inTraversalStack,
	const __local Scene* scene,
	const Ray* ray,
	bool hitAny,
	float maxT,
	int* outTriangleIndex,
	float* outT,
	float2* outUV,
#ifdef COUNT_TRAVERSAL
	const __global float** outInvTransform,
	int* count)
#else
	const __global float** outInvTransform)
#endif
{
	int triangleIndex;
	float closestT = maxT;
	float2 closestUV;
	const __global float* closestMatrix;
#ifdef USE_BVH

#ifdef COUNT_TRAVERSAL
	if (count) *count = 0;
#endif
	// Check mesh intersection using BVH traversal
	//unsigned int subBvhStack[32];
	__global uint* subBvhStack = &inTraversalStack[get_global_id(0) * 32];
	int subBvhStackPtr = 0;

	// Traverse top level BVH and add relevant sub-BVH's to the "sub BVH" stacks
	__local unsigned int topLevelBvhStackLocal[10 * 64];
	__local unsigned int* topLevelBvhStack = &topLevelBvhStackLocal[
		get_local_id(0) * 10];
	//unsigned int topLevelBvhStack[16];
	unsigned int topLevelBvhStackPtr = 0;
	topLevelBvhStack[topLevelBvhStackPtr++] = scene->topLevelBvhRoot;

	while (true)
	{
		unsigned int subBvhNodeId = -1;
		const __global float* invTransform;
		Ray transformedRay;
		
		while (topLevelBvhStackPtr > 0)
		{

#ifdef COUNT_TRAVERSAL
	if (count) *count += 1;
#endif
			const __global TopBvhNode* node = &scene->topLevelBvh[topLevelBvhStack[--topLevelBvhStackPtr]];
			if (!intersectRayTopBvh(ray, node, closestT))
				continue;

			if (node->isLeaf)
			{
				//Ray transformedRay;
				transformedRay.origin = matrixMultiply(node->invTransform, (float4)(ray->origin, 1.0f)).xyz;
				transformedRay.direction = matrixMultiply(node->invTransform, (float4)(ray->direction, 0.0f)).xyz;
				invTransform = node->invTransform;
				subBvhNodeId = node->subBvh;

#ifdef NO_PARALLEL_RAYS
				if (transformedRay.direction.x == 0.0f)
					transformedRay.direction.x = FLT_MIN;
				if (transformedRay.direction.y == 0.0f)
					transformedRay.direction.y = FLT_MIN;
				if (transformedRay.direction.z == 0.0f)
					transformedRay.direction.z = FLT_MIN;

				if (transformedRay.origin.x == 0.0f)
					transformedRay.origin.x = -FLT_MIN;
				if (transformedRay.origin.y == 0.0f)
					transformedRay.origin.y = -FLT_MIN;
				if (transformedRay.origin.z == 0.0f)
					transformedRay.origin.z = -FLT_MIN;
#endif

				break;
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

		if (subBvhNodeId == -1)
			break;

		while (true)
		{
			SubBvhNode node = scene->subBvh[subBvhNodeId];

			if (node.triangleCount != 0)// isLeaf()
			{
				for (int i = 0; i < node.triangleCount; i++)
				{
					float t;
					float2 uv;

					VertexData vertices[3];
					TriangleData triangle = scene->triangles[node.firstTriangleIndex + i];
					getVertices(vertices, triangle.indices, scene);
#ifdef COUNT_TRAVERSAL
					if (count) *count += 1;
#endif
					if (intersectRayTriangle(&transformedRay, vertices, &t, &uv) && t < closestT)
					{
						if (hitAny)
							return true;

						triangleIndex = node.firstTriangleIndex + i;
						closestT = t;
						closestUV = uv;
						closestMatrix = invTransform;
					}
				}

				if (subBvhStackPtr > 0)
					subBvhNodeId = subBvhStack[--subBvhStackPtr];// Pop
				else
					break;
			} else {
				// Ordered traversal
				SubBvhNode left = scene->subBvh[node.leftChildIndex + 0];
				SubBvhNode right = scene->subBvh[node.leftChildIndex + 1];

#ifdef COUNT_TRAVERSAL
	if (count) *count += 2;
#endif
				float leftDist, rightDist;
				bool leftVis = intersectRaySubBvh(&transformedRay, &left, closestT, &leftDist);
				bool rightVis = intersectRaySubBvh(&transformedRay, &right, closestT, &rightDist);

				if (leftVis && rightVis)
				{
					if (leftDist < rightDist)
					{
						subBvhStack[subBvhStackPtr++] = node.leftChildIndex + 1;
						subBvhNodeId = node.leftChildIndex + 0;
					} else {
						subBvhStack[subBvhStackPtr++] = node.leftChildIndex + 0;
						subBvhNodeId = node.leftChildIndex + 1;
					}// Ordered of traversal
				} else if (leftVis)
				{
					subBvhNodeId = node.leftChildIndex;
				} else if (rightVis)
				{
					subBvhNodeId = node.leftChildIndex + 1;
				} else {
					if (subBvhStackPtr > 0)
						subBvhNodeId = subBvhStack[--subBvhStackPtr];// Pop
					else
						break;
				}
			}// If/else node contains triangles
		}// Sub bvh traversal
	}// Traversal

#else// USE_BVH
	for (unsigned int i = 0; i < scene->numTriangles; i++)
	{
		float t;
		float2 uv;

		VertexData vertices[3];
		TriangleData triangle = scene->triangles[i];
		getVertices(vertices, triangle.indices, scene);
		if (intersectRayTriangle(ray, vertices, &t, &uv) && t < closestT)
		{
			if (hitAny)
				return true;

			triangleIndex = i;
			closestT = t;
			closestUV = uv;
			closestMatrix = &scene->topLevelBvh[scene->topLevelBvhRoot].invTransform;
		}
	}
#endif// USE_BVH

	if (closestT != maxT)// We did not hit anysubg
	{
		if (outTriangleIndex)
			*outTriangleIndex = triangleIndex;
		if (outT)
			*outT = closestT;
		if (outUV)
			*outUV = closestUV;
		if (outInvTransform)
			*outInvTransform = closestMatrix;
		return true;
	} else {// We did not intersect with any triangle
		return false;
	}
}

#endif// __SCENE_CL