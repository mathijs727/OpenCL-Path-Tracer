#define NO_PARALLEL_RAYS// When a ray is parallel to axis, the intersection tests are really slow
#define USE_BVH
//#define COUNT_TRAVERSAL// Define here so it can be accessed by include files

#define MAX_ITERATIONS 1

#include <clRNG/mrg31k3p.clh>
#include "shapes.cl"
#include "material.cl"
#include "scene.cl"
#include "light.cl"
#include "stack.cl"
#include "gamma.cl"
#include "bvh.cl"
#include "shading.cl"
#include "camera.cl"

typedef struct
{
	// Camera
	Camera camera;

	// Scene
	uint numEmissiveTriangles;
	uint topLevelBvhRoot;

	uint iteration;
} KernelData;

__kernel void generatePrimaryRays(
	__global ShadingData* outRays,
	__global KernelData* inputData,
	__global clrngMrg31k3pHostStream* randomStreams)
{
	size_t x = get_global_id(0);
	size_t y = get_global_id(1);
	size_t gid = y * get_global_size(0) + x;

	clrngMrg31k3pStream randomStream;
	clrngMrg31k3pCopyOverStreamsFromGlobal(1, &randomStream, &randomStreams[gid]);

	outRays[gid].ray = generateRayThinLens(&inputData->camera, x, y, &randomStream);
	outRays[gid].multiplier = (float3)(1, 1, 1);
	outRays[gid].flags = SHADINGFLAGS_LASTSPECULAR;
	outRays[gid].outputPixel = gid;

	// Store random streams
	clrngMrg31k3pCopyOverStreamsToGlobal(1, &randomStreams[gid], &randomStream);
}

__kernel void intersectShadows(
	__global float3* outputPixels,
	__global ShadingData* inShadowRays,

	__global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global EmissiveTriangle* emissiveTriangles,
	__global Material* materials,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh)
{
	size_t gid = get_global_id(0);
	ShadingData shadowData = inShadowRays[gid];
	if (shadowData.flags & SHADINGFLAGS_HASFINISHED)
		return;

	Scene scene;
	loadScene(
		vertices,
		triangles,
		materials,
		inputData->numEmissiveTriangles,
		emissiveTriangles,
		subBvh,
		inputData->topLevelBvhRoot,
		topLevelBvh,
		&scene);

	bool hit = traceRay(
		&scene,
		&shadowData.ray,
		true,
		shadowData.rayLength,
		NULL,
		NULL,
		NULL,
		NULL);
	if (!hit)
	{
		outputPixels[gid] += shadowData.multiplier;
	}
}

__kernel void intersectAndShade(
	__global float3* outputPixels,
	__global ShadingData* outRays,
	__global ShadingData* outShadowRays,
	__global ShadingData* inRays,

	__global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global EmissiveTriangle* emissiveTriangles,
	__global Material* materials,
	__read_only image2d_array_t textures,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh,
	__global clrngMrg31k3pHostStream* randomStreams)
{
	size_t gid = get_global_id(0);
	ShadingData shadingData = inRays[gid];
	ShadingData outShadingData;
	ShadingData outShadowShadingData;
	outShadingData.outputPixel = inRays->outputPixel;
	outShadowShadingData.outputPixel = inRays->outputPixel;

	if (!(shadingData.flags & SHADINGFLAGS_HASFINISHED))
	{
		clrngMrg31k3pStream randomStream;
		clrngMrg31k3pCopyOverStreamsFromGlobal(1, &randomStream, &randomStreams[gid]);

		Scene scene;
		loadScene(
			vertices,
			triangles,
			materials,
			inputData->numEmissiveTriangles,
			emissiveTriangles,
			subBvh,
			inputData->topLevelBvhRoot,
			topLevelBvh,
			&scene);

		// Trace rays
		int triangleIndex;
		float t;
		float2 uv;
		const __global float* invTransform;
		bool hit = traceRay(
			&scene,
			&shadingData.ray,
			false,
			INFINITY,
			&triangleIndex,
			&t,
			&uv,
			&invTransform);
		float3 intersection = shadingData.ray.origin + shadingData.ray.direction * t;
		float normalTransform[16];
		matrixTranspose(invTransform, normalTransform);

		if (hit)
		{
			//outputPixels[gid] += (float3)(0, 0.5, 0);
			outputPixels[gid] += neeShading(
				&scene,
				triangleIndex,
				intersection,
				shadingData.ray.direction,
				normalTransform,
				uv,
				textures,
				&randomStream,
				&shadingData,
				&outShadingData,
				&outShadowShadingData);
			//outputPixels[gid] += outShadowShadingData.multiplier;
		} else {
			outShadingData.flags = SHADINGFLAGS_HASFINISHED;
			outShadowShadingData.flags = SHADINGFLAGS_HASFINISHED;
		}

		outRays[gid] = outShadingData;
		outShadowRays[gid] = outShadowShadingData;

		// Store random streams
		clrngMrg31k3pCopyOverStreamsToGlobal(1, &randomStreams[gid], &randomStream);
	}
}

/*__kernel void traceRays(
	__global float3* output,
	__global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global EmissiveTriangle* emissiveTriangles,
	__global Material* materials,
	__read_only image2d_array_t textures,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh,
	__global clrngMrg31k3pHostStream* randomStreams) {
	//__read_only image2d_array_t textures) {
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0);
	int gid = y * width + x;
	bool leftSide = (x < width / 2);

	// Read random stream
	clrngMrg31k3pStream privateStream;
	clrngMrg31k3pCopyOverStreamsFromGlobal(1, &privateStream, &randomStreams[gid]);

	Scene scene;
	loadScene(
		vertices,
		triangles,
		materials,
		inputData->numEmissiveTriangles,
		emissiveTriangles,
		subBvh,
		inputData->topLevelBvhRoot,
		topLevelBvh,
		&scene);

	ShadingData shadingData;

	float3 accumulatedColour = (float3)(0, 0, 0);
	for (int i = 0; i < inputData->raysPerPass; i++)
	{
		float corX = (leftSide ? x : x - width / 2);
		Ray cameraRay = generateRayThinLens(&inputData->camera, corX, y, &privateStream);
		
		shadingData.ray = cameraRay;
		shadingData.multiplier = (float3)(1, 1, 1);
		shadingData.flags = SHADINGFLAGS_LASTSPECULAR;

		bool shouldFinish = false;
		for (int iteration = 0; iteration < MAX_ITERATIONS && !shouldFinish; ++iteration)
		{
			int triangleIndex;
			float t;
			float2 uv;
			const __global float* invTransform;
			bool hit = traceRay(&scene, &shadingData.ray, false, INFINITY, &triangleIndex, &t, &uv, &invTransform);
			if (hit)
			{
				float3 intersection = t * shadingData.ray.direction + shadingData.ray.origin;
				float normalTransform[16];
				matrixTranspose(invTransform, normalTransform);
				if (leftSide)
				{
					accumulatedColour += naiveShading(
						&scene,
						triangleIndex,
						intersection,
						shadingData.ray.direction,
						normalTransform,
						uv,
						textures,
						&privateStream,
						&shadingData);
				} else {
					accumulatedColour += neeIsShading(
						&scene,
						triangleIndex,
						intersection,
						shadingData.ray.direction,
						normalTransform,
						uv,
						textures,
						&privateStream,
						&shadingData);
				}
			}
			shouldFinish = !hit || (shadingData.flags & SHADINGFLAGS_HASFINISHED);
		}
	}
	output[gid] += accumulatedColour;

	// Store random streams
	clrngMrg31k3pCopyOverStreamsToGlobal(1, &randomStreams[gid], &privateStream);
}*/
