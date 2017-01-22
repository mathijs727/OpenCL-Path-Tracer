#define NO_PARALLEL_RAYS// When a ray is parallel to axis, the intersection tests are really slow
#define USE_BVH
//#define COUNT_TRAVERSAL// Define here so it can be accessed by include files
#define MAX_ITERATIONS 4

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

typedef struct
{
	float2 uv;
	const __global float* invTransform;
	int triangleIndex;
	float t;
	bool hit;
} ShadingData;


__kernel void generatePrimaryRays(
	__global RayData* outRays,
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
	clrngLfsr113CopyOverStreamsToGlobal(1, &randomStreams[gid], &randomStream);

	if (gid == 0)
	{
		//inputData->rayOffset = newRays;
		inputData->newRays = newRays;
	}
}

__kernel void intersectShadows(
	__global float3* outputPixels,
	__global RayData* inShadowRays,

	volatile __global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global EmissiveTriangle* emissiveTriangles,
	__global Material* materials,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh)
{
	size_t gid = get_global_id(0);
	RayData shadowData = inShadowRays[gid];
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

__kernel void intersectWalk(
	//__global RayData* outRays,
	//__global RayData* outShadowRays,
	__global ShadingData* outShadingData,

	__global RayData* inRays,
	volatile __global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	//__global EmissiveTriangle* emissiveTriangles,
	//__global Material* materials,
	//__read_only image2d_array_t textures,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh,
	__global float3* outputPixels)
	//__global clrngLfsr113HostStream* randomStreams)
{
	size_t gid = get_global_id(0);
	//RayData outRayData;
	//RayData outShadowRayData;
	RayData rayData = inRays[gid];

	ShadingData shadingData;
	shadingData.hit = false;

	if (gid < (inputData->numInRays + inputData->newRays) && rayData.flags != SHADINGFLAGS_HASFINISHED)
	{
		//outRayData.outputPixel = rayData.outputPixel;
		//outShadowRayData.outputPixel = rayData.outputPixel;
		//outRayData.flags = 0;
		//outShadowRayData.flags = 0;

		//clrngLfsr113Stream randomStream;
		//clrngLfsr113CopyOverStreamsFromGlobal(1, &randomStream, &randomStreams[gid]);

		Scene scene;
		loadScene(
			vertices,
			triangles,
			NULL,// Dont need materials for intersection
			inputData->numEmissiveTriangles,
			NULL,// Dont need emissive triangles for intersection
			subBvh,
			inputData->topLevelBvhRoot,
			topLevelBvh,
			&scene);

		// Trace rays
		//float2 uv;
		//const __global float* invTransform;
		//int triangleIndex;
		//float t;
		shadingData.hit = traceRay(
			&scene,
			&rayData.ray,
			false,
			INFINITY,
			&shadingData.triangleIndex,
			&shadingData.t,
			&shadingData.uv,
			&shadingData.invTransform);

		/*shadingData.intersection = rayData.ray.origin + rayData.ray.direction * t;
		shadingData.direction = rayData.ray.direction;
		shadingData.uv = uv;
		shadingdata.invTransform = invTransform;
		shadingData.triangleIndex = triangleIndex;
		shadingData.hit = hit;*/

		/*float normalTransform[16];
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
				&outRayData,
				&outShadowRayData);
			outRayData.numBounces = shadingData.numBounces + 1;
			//outRay = (outRayData.numBounces < 5);
			//outputPixels[shadingData.outputPixel] += outRayData.ray.direction;
		}*/

		// Store random streams
		//clrngLfsr113CopyOverStreamsToGlobal(1, &randomStreams[gid], &randomStream);
	} 

	outShadingData[gid] = shadingData;

	/*int index = workgroup_counter_inc(&inputData->numOutRays, shadingData.hit);//atomic_inc(&inputData->numOutRays);
	if (shadingData.hit)
	{
		if (outRayData.numBounces >= MAX_ITERATIONS)
			outRayData.flags = SHADINGFLAGS_HASFINISHED;

		outRays[index] = outRayData;
		outShadowRays[index] = outShadowRayData;
	}*/
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
	__read_only image2d_array_t textures,
	__global clrngLfsr113HostStream* randomStreams)
{
	size_t gid = get_global_id(0);
	RayData outRayData;
	RayData outShadowRayData;
	const __global RayData* rayData = &inRays[gid];// TODO: use pointer to safe registers
	const __global ShadingData* shadingData = &inShadingData[gid];// TODO: use pointer to safe registers
	bool active = false;


	if (gid < (inputData->numInRays + inputData->newRays) &&
		rayData->flags != SHADINGFLAGS_HASFINISHED &&
		shadingData->hit)
	{		
		outRayData.outputPixel = rayData->outputPixel;
		outShadowRayData.outputPixel = rayData->outputPixel;
		outRayData.flags = 0;
		outShadowRayData.flags = 0;
		outRayData.numBounces = rayData->numBounces + 1;

		float3 intersection = rayData->ray.origin + shadingData->t * rayData->ray.direction;

		// Load scene
		Scene scene;
		loadScene(
			vertices,
			triangles,
			materials,// Dont need materials for intersection
			inputData->numEmissiveTriangles,
			emissiveTriangles,// Dont need emissive triangles for intersection
			NULL,
			inputData->topLevelBvhRoot,
			NULL,
			&scene);

		// Load random streams
		clrngLfsr113Stream randomStream;
		clrngLfsr113CopyOverStreamsFromGlobal(1, &randomStream, &randomStreams[gid]);

		outputPixels[rayData->outputPixel] += neeShading(
			&scene,
			shadingData->triangleIndex,
			intersection,
			normalize(rayData->ray.direction),
			shadingData->invTransform,
			shadingData->uv,
			textures,
			&randomStream,
			rayData,
			&outRayData,
			&outShadowRayData);

		// Store random streams
		clrngLfsr113CopyOverStreamsToGlobal(1, &randomStreams[gid], &randomStream);
		active = true;
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
	data->numInRays = data->numOutRays;
	data->numOutRays = 0;
	data->numShadowRays = 0;
	data->rayOffset += data->newRays;
	data->newRays = 0;
}
