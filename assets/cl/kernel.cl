//#define NO_SHADOWS
#define USE_BVH
//#define COUNT_TRAVERSAL// Define here so it can be accessed by include files

// When a ray is parallel to axis, the intersection tests are really slow
#define NO_PARALLEL_RAYS

#include <clRNG/mrg31k3p.clh>
#include "shapes.cl"
#include "material.cl"
#include "scene.cl"
#include "light.cl"
#include "stack.cl"
#include "gamma.cl"
#include "bvh.cl"
#include "shading.cl"

typedef struct
{
	// Camera
	float3 eye;// Position of the camera "eye"
	float3 screen;// Left top of screen in world space
	float3 u_step;// Horizontal distance between pixels in world space
	float3 v_step;// Vertical distance between pixels in world space
	uint width;// Render target width

	// Scene
	uint numVertices, numTriangles, numEmmisiveTriangles, numLights;
	uint topLevelBvhRoot;

	uint raysPerPass;
} KernelData;

__kernel void traceRays(
	__global float3* output,
	__global KernelData* inputData,
	__global VertexData* vertices,
	__global TriangleData* triangles,
	__global uint* emmisiveTriangles,
	__global Material* materials,
	__read_only image2d_array_t textures,
	__global Light* lights,
	__global SubBvhNode* subBvh,
	__global TopBvhNode* topLevelBvh,
	__global clrngMrg31k3pHostStream* randomStreams) {
	//__read_only image2d_array_t textures) {
	int x = get_global_id(0);
	int y = get_global_id(1);
	int gid = y * get_global_size(0) + x;

	// Read random stream
	clrngMrg31k3pStream privateStream;
	clrngMrg31k3pCopyOverStreamsFromGlobal(1, &privateStream, &randomStreams[gid]);

	Scene scene;
	loadScene(
		inputData->numVertices,
		vertices,
		inputData->numTriangles,
		triangles,
		inputData->numEmmisiveTriangles,
		emmisiveTriangles,
		materials,
		inputData->numLights,
		lights,
		subBvh,
		inputData->topLevelBvhRoot,
		topLevelBvh,
		&scene);
	
	float3 accumulatedColour = (float3)(0, 0, 0);
	for (int i = 0; i < inputData->raysPerPass; i++)
	{
		float3 screenPoint = inputData->screen + \
			inputData->u_step * (float)x + inputData->v_step * (float)y;	
		screenPoint += (float)clrngMrg31k3pRandomU01(&privateStream) * inputData->u_step;
		screenPoint += (float)clrngMrg31k3pRandomU01(&privateStream) * inputData->v_step;

		Ray ray;
		ray.origin = inputData->eye;
		ray.direction = normalize(screenPoint - inputData->eye);

		int triangleIndex;
		float t;
		float2 uv;
		const __global float* invTransform;
		bool hit = traceRay(&scene, &ray, false, INFINITY, &triangleIndex, &t, &uv, &invTransform);
		if (hit)
		{
			float3 intersection = t * ray.direction + ray.origin;
			float3 outColour = slide16Shading(
				&scene,
				triangleIndex,
				intersection,
				ray.direction,
				invTransform,
				uv,
				textures,
				&privateStream);
			accumulatedColour += outColour;
		}
	}
	output[y * inputData->width + x] += accumulatedColour;

	// Store random streams
	clrngMrg31k3pCopyOverStreamsToGlobal(1, &randomStreams[gid], &privateStream);
	/*StackItem item;
	item.ray.origin = inputData->eye;
	item.ray.direction = normalize(screenPoint - inputData->eye);
	item.multiplier = (float3)(1.0f, 1.0f, 1.0f);

	uint iterCount = 0;
	float3 outColor = (float3)(0.0f, 0.0f, 0.0f);
	Stack stack;
	StackInit(&stack);
	StackPush(&stack, &item);
	while (!StackEmpty(&stack))
	{
		StackItem item;
		StackPop(&stack, &item);

		int triangleIndex;
		float t;
		float2 uv;
		const __global float* invTransform;
		bool hit = traceRay(&scene, &item.ray, false, INFINITY, &triangleIndex, &t, &uv, &invTransform);
		
		if (hit)
		{
			float3 direction = item.ray.direction;
			float3 intersection = t * item.ray.direction + item.ray.origin;

			outColor += whittedShading(
				&scene,
				triangleIndex,
				intersection,
				invTransform,
				direction,
				uv,
				textures,
				item.multiplier,
				&stack);
		}
		if (++iterCount >= MAX_ITERATIONS)
		{
			break;
		}
	}*/

}