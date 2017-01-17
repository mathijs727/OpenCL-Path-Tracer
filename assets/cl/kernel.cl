#define NO_PARALLEL_RAYS// When a ray is parallel to axis, the intersection tests are really slow
#define USE_BVH
//#define COUNT_TRAVERSAL// Define here so it can be accessed by include files
#define MAX_ITERATIONS 3

#include <clRNG/lfsr113.clh>
#include "shapes.cl"
#include "material.cl"
#include "scene.cl"
#include "light.cl"
#include "gamma.cl"
#include "bvh.cl"
#include "shading.cl"
#include "camera.cl"
#include "atomic.cl"

typedef struct
{
	// Camera
	Camera camera;

	// Scene
	uint numEmissiveTriangles;
	uint topLevelBvhRoot;

	// Used for ray generation
	uint rayOffset;
	uint scrWidth;
	uint scrHeight;

	// Used for compaction
	uint numInRays;
	uint numOutRays;
	uint numShadowRays;
	uint maxRays;
	uint newRays;
} KernelData;



__kernel void generatePrimaryRays(
	__global ShadingData* outRays,
	volatile __global KernelData* inputData,
	__global clrngLfsr113HostStream* randomStreams)
{
	size_t gid = get_global_id(0);
	uint rayIndex = inputData->rayOffset + gid;

	// Stop when we've created all the rays
	uint totalRays = inputData->scrWidth * inputData->scrHeight;
	uint newRays = inputData->maxRays - inputData->numInRays;
	if ((inputData->rayOffset + newRays) > totalRays)
		newRays -= inputData->rayOffset + newRays - totalRays;

	// Dont overflow / write out of bounds
	if (gid >= newRays)
		return;

	clrngLfsr113Stream randomStream;
	clrngLfsr113CopyOverStreamsFromGlobal(1, &randomStream, &randomStreams[gid]);

	uint x = rayIndex % inputData->scrWidth;
	uint y = rayIndex / inputData->scrWidth;

	size_t outIndex = inputData->numInRays + gid;
	outRays[outIndex].ray = generateRayThinLens(
		&inputData->camera,
		x,
		y,
		(float)inputData->scrWidth,
		(float)inputData->scrHeight,
		&randomStream);
	outRays[outIndex].multiplier = (float3)(1, 1, 1);
	outRays[outIndex].flags = SHADINGFLAGS_LASTSPECULAR;
	outRays[outIndex].outputPixel = rayIndex;
	outRays[outIndex].numBounces = 0;

	// Store random streams
	clrngLfsr113CopyOverStreamsToGlobal(1, &randomStreams[gid], &randomStream);

	if (gid == 0)
	{
		//inputData->rayOffset = newRays;
		inputData->newRays = newRays;
	}
}

__kernel void intersectShadows(
	__global float3* outputPixels,
	__global ShadingData* inShadowRays,

	volatile __global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global EmissiveTriangle* emissiveTriangles,
	__global Material* materials,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh)
{
	size_t gid = get_global_id(0);
	ShadingData shadowData = inShadowRays[gid];
	if (gid >= inputData->numOutRays)
		return;
	inShadowRays[gid].flags = SHADINGFLAGS_HASFINISHED;
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
		shadowData.rayLength,//-0.01f,
		NULL,
		NULL,
		NULL,
		NULL);
	if (!hit)
	{
		outputPixels[shadowData.outputPixel] += shadowData.multiplier;
	}
}

__kernel void intersectAndShade(
	__global float3* outputPixels,
	__global ShadingData* outRays,
	__global ShadingData* outShadowRays,
	__global ShadingData* inRays,

	volatile __global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global EmissiveTriangle* emissiveTriangles,
	__global Material* materials,
	__read_only image2d_array_t textures,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh,
	__global clrngLfsr113HostStream* randomStreams)
{
	size_t gid = get_global_id(0);
	bool hit = false;
	ShadingData outShadingData;
	ShadingData outShadowShadingData;
	ShadingData shadingData = inRays[gid];

	if (gid < (inputData->numInRays + inputData->newRays) && shadingData.flags != SHADINGFLAGS_HASFINISHED)
	{
		outShadingData.outputPixel = shadingData.outputPixel;
		outShadowShadingData.outputPixel = shadingData.outputPixel;
		outShadingData.flags = 0;
		outShadowShadingData.flags = 0;
		

		clrngLfsr113Stream randomStream;
		clrngLfsr113CopyOverStreamsFromGlobal(1, &randomStream, &randomStreams[gid]);

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
		hit = traceRay(
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
			outputPixels[shadingData.outputPixel] += neeShading(
				&scene,
				triangleIndex,
				intersection,
				normalize(shadingData.ray.direction),
				normalTransform,
				uv,
				textures,
				&randomStream,
				&shadingData,
				&outShadingData,
				&outShadowShadingData);
			outShadingData.numBounces = shadingData.numBounces + 1;
			//outRay = (outShadingData.numBounces < 5);
			//outputPixels[shadingData.outputPixel] += outShadingData.ray.direction;
		}

		// Store random streams
		clrngLfsr113CopyOverStreamsToGlobal(1, &randomStreams[gid], &randomStream);
	} 

	int index = workgroup_counter_inc(&inputData->numOutRays, hit);//atomic_inc(&inputData->numOutRays);
	if (hit)
	{
		if (outShadingData.numBounces >= MAX_ITERATIONS)
			outShadingData.flags = SHADINGFLAGS_HASFINISHED;

		outRays[index] = outShadingData;
		outShadowRays[index] = outShadowShadingData;
	}
}

__kernel void updateKernelData(
	volatile __global KernelData* data)
{
	data->numInRays = data->numOutRays;
	data->numOutRays = 0;
	data->numShadowRays = 0;
	data->rayOffset += data->newRays;
	data->newRays = 0;
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
	__global clrngLfsr113HostStream* randomStreams) {
	//__read_only image2d_array_t textures) {
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0);
	int gid = y * width + x;
	bool leftSide = (x < width / 2);

	// Read random stream
	clrngLfsr113Stream privateStream;
	clrngLfsr113CopyOverStreamsFromGlobal(1, &privateStream, &randomStreams[gid]);

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
	clrngLfsr113CopyOverStreamsToGlobal(1, &randomStreams[gid], &privateStream);
}*/
