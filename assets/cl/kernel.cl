//#define NO_SHADOWS

// When a ray is parallel to axis, the intersection tests are really slow
#define NO_PARALLEL_RAYS

#define USE_BVH
//#define COUNT_TRAVERSAL// Define here so it can be accessed by include files

#include "shapes.cl"
#include "material.cl"
#include "scene.cl"
#include "light.cl"
#include "stack.cl"
#include "gamma.cl"
#include "bvh.cl"
#include "shading.cl"

#define MAX_ITERATIONS 4

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
	__global TopBvhNode* topLevelBvh) {
	//__read_only image2d_array_t textures) {
	int x = get_global_id(0);
	int y = get_global_id(1);

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
	
	float3 screenPoint = inputData->screen + \
		inputData->u_step * (float)x + inputData->v_step * (float)y;	

	StackItem item;
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
#ifdef COUNT_TRAVERSAL
		int bvhCount;
		bool hit = traceRay(&scene, &item.ray, false, INFINITY, &triangleIndex, &t, &uv, &invTransform, &bvhCount);
#else
		bool hit = traceRay(&scene, &item.ray, false, INFINITY, &triangleIndex, &t, &uv, &invTransform);
#endif
		
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

#ifdef COUNT_TRAVERSAL
		outColor += bvhCount * (float3)(0.f, 0.001f, 0.f);
#endif

		if (++iterCount >= MAX_ITERATIONS)
		{
			break;
		}
	}

	//outColor = accurateLinearToSRGB(outColor);
	//write_imagef(output, (int2)(x, y), (float4)(outColor, 1.0f));
	output[y * inputData->width + x] = outColor;
}