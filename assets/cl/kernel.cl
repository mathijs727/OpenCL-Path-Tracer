#define NO_PARALLEL_RAYS// When a ray is parallel to axis, the intersection tests are really slow
#define USE_BVH
//#define COUNT_TRAVERSAL// Define here so it can be accessed by include files
#define MAX_ITERATIONS 4

//#define COMPARE_SHADING
#define CLRNG_SINGLE_PRECISION

#include "random.cl"
#include "shapes.cl"
#include "material.cl"
#include "scene.cl"
#include "light.cl"
#include "gamma.cl"
#include "bvh.cl"
#include "shading.cl"
#include "camera.cl"
#include "atomic.cl"
#include "kernel_data.cl"
//#include "cubemap.cl"
#include "skydome.cl"


__kernel void generatePrimaryRays(
	__global RayData* outRays,
	volatile __global KernelData* inputData,
	__global randHostStream* randomStreams)
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

	randStream randomStream;
	randCopyOverStreamsFromGlobal(1, &randomStream, &randomStreams[gid]);

	uint x = rayIndex % inputData->scrWidth;
	uint y = rayIndex / inputData->scrWidth;

#ifdef COMPARE_SHADING
	if (x >= inputData->scrWidth / 2)
		x -= inputData->scrWidth / 2; 
#endif
	size_t outIndex = inputData->numInRays + gid;
	if (inputData->camera.thinLenseEnabled)
	{
		outRays[outIndex].ray = generateRayThinLens(
			&inputData->camera,
			x,
			y,
			(float)inputData->scrWidth,
			(float)inputData->scrHeight,
			&randomStream);
	} else {
		outRays[outIndex].ray = generateRayPinhole(
			&inputData->camera,
			x,
			y,
			(float)inputData->scrWidth,
			(float)inputData->scrHeight,
			&randomStream);
	}
	outRays[outIndex].multiplier = (float3)(1, 1, 1);
	outRays[outIndex].flags = SHADINGFLAGS_LASTSPECULAR;
	outRays[outIndex].outputPixel = rayIndex;
	outRays[outIndex].numBounces = 0;

	// Store random streams
	randCopyOverStreamsToGlobal(1, &randomStreams[gid], &randomStream);

	if (gid == 0)
	{
		//inputData->rayOffset = newRays;
		inputData->newRays = newRays;
	}
}

__kernel void intersectShadows(
	__global float3* outputPixels,

	__global RayData* inShadowRays,
	__global uint* inTraversalStack,
	volatile __global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh)
{
	size_t gid = get_global_id(0);

	__local Scene scene;
	if (get_local_id(0) == 0)
	{
		loadScene(
			vertices,
			triangles,
			NULL,
			0,
			NULL,
			subBvh,
			inputData->topLevelBvhRoot,
			topLevelBvh,
			&scene);
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	RayData shadowData = inShadowRays[gid];
	if (gid >= inputData->numOutRays)
		return;
	inShadowRays[gid].flags = SHADINGFLAGS_HASFINISHED;
	if (shadowData.flags & SHADINGFLAGS_HASFINISHED)
		return;

	bool hit = traceRay(
		inTraversalStack,
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

__kernel void intersectWalk(
	__global ShadingData* outShadingData,

	__global RayData* inRays,
	__global uint* inTraversalStack,
	volatile __global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh,
	__global float3* outputPixels)
{
	size_t gid = get_global_id(0);

	__local Scene scene;
	if (get_local_id(0) == 0)
	{
		loadScene(
			vertices,
			triangles,
			NULL,// Dont need materials for intersection
			0,
			NULL,// Dont need emissive triangles for intersection
			subBvh,
			inputData->topLevelBvhRoot,
			topLevelBvh,
			&scene);
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	RayData rayData = inRays[gid];
	ShadingData shadingData;
	shadingData.hit = false;

	if (gid < (inputData->numInRays + inputData->newRays) && !(rayData.flags & SHADINGFLAGS_HASFINISHED))
	{
		// Trace rays
		shadingData.hit = traceRay(
			inTraversalStack,
			&scene,
			&rayData.ray,
			false,
			INFINITY,
			&shadingData.triangleIndex,
			&shadingData.t,
			&shadingData.uv,
			&shadingData.invTransform);
	}

	outShadingData[gid] = shadingData;
}

__kernel void shade(
	__global float3* outputPixels,
	__global RayData* outRays,
	__global RayData* outShadowRays,

	__global RayData* inRays,
	__global ShadingData* inShadingData,
	volatile __global KernelData* inputData,

	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global EmissiveTriangle* emissiveTriangles,
	__global Material* materials,
	__read_only image2d_array_t materialTextures,
	__read_only image2d_array_t skydomeTextures,
	__global randHostStream* randomStreams)
{
	size_t gid = get_global_id(0);
	RayData outRayData;
	RayData outShadowRayData;
	const __global RayData* rayData = &inRays[gid];// TODO: use pointer to safe registers
	const __global ShadingData* shadingData = &inShadingData[gid];// TODO: use pointer to safe registers
	bool active = false;

	__local Scene scene;
	if (get_local_id(0) == 0)
	{
		loadScene(
			vertices,
			triangles,
			materials,// Dont need materials for intersection
			inputData->numEmissiveTriangles,
			emissiveTriangles,// Dont need emissive triangles for intersection
			NULL,
			0,
			NULL,
			&scene);
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	if (gid < (inputData->numInRays + inputData->newRays) &&
		!(rayData->flags & SHADINGFLAGS_HASFINISHED))
	{
		if (shadingData->hit)
		{
			outRayData.outputPixel = rayData->outputPixel;
			outShadowRayData.outputPixel = rayData->outputPixel;
			outRayData.flags = 0;
			outShadowRayData.flags = 0;
			outRayData.numBounces = rayData->numBounces + 1;
			outRayData.pdf = 0;

			float3 intersection = rayData->ray.origin + shadingData->t * rayData->ray.direction;

			// Load random streams
			randStream randomStream;
			randCopyOverStreamsFromGlobal(1, &randomStream, &randomStreams[gid]);

#ifdef COMPARE_SHADING
			if ((rayData->outputPixel % inputData->scrWidth) < inputData->scrWidth / 2)
			{
				outputPixels[rayData->outputPixel] += neeMisShading(
					&scene,
					shadingData->triangleIndex,
					intersection,
					normalize(rayData->ray.direction),
					shadingData->t,
					shadingData->invTransform,
					shadingData->uv,
                    materialTextures,
					&randomStream,
					rayData,
					&outRayData,
					&outShadowRayData);
			} else
#endif
			{
				outputPixels[rayData->outputPixel] += neeIsShading(
					&scene,
					shadingData->triangleIndex,
					intersection,
					normalize(rayData->ray.direction),
					shadingData->t,
					shadingData->invTransform,
					shadingData->uv,
                    materialTextures,
					&randomStream,
					rayData,
					&outRayData,
					&outShadowRayData);
			}

			active = true;
			// Store random streams
			randCopyOverStreamsToGlobal(1, &randomStreams[gid], &randomStream);
		} else {
			// We missed the scene but have a skydome to fall back to
			float3 c = readSkydome(normalize(rayData->ray.direction), skydomeTextures);
			outputPixels[rayData->outputPixel] += rayData->multiplier * c;
		}
	}

	int index = workgroup_counter_inc(&inputData->numOutRays, active);
	if (active)
	{
		if (outRayData.numBounces >= MAX_ITERATIONS)
			outRayData.flags = SHADINGFLAGS_HASFINISHED;

		outRays[index] = outRayData;
		outShadowRays[index] = outShadowRayData;
	}
}

__kernel void updateKernelData(
	volatile __global KernelData* data)
{
	// This is executed in its own kernel to prevent race conditions.
	// If we were to set a variable to 0 from thread 0 and then increment;
	//  then a race condition would occur between work groups. If workgroup 1
	//  would start execution before the counter is reset by workgroup 0, than
	//  the results of workgroup 1 are incorrect. This cannot be fixed by
	//  using barriers since those are only valid within a work group.
	data->numInRays = data->numOutRays;
	data->numOutRays = 0;
	data->numShadowRays = 0;
	data->rayOffset += data->newRays;
	data->newRays = 0;
}
