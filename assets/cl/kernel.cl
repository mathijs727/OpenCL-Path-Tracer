#include "shapes.cl"
#include "rawshapes.cl"
#include "material.cl"
#include "scene.cl"
#include "light.cl"
#include "stack.cl"

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
	int numSpheres, numPlanes, numVertices, numTriangles, numLights;
} KernelData;

__kernel void hello(
	__write_only image2d_t output,
	__read_only __global KernelData* inputData,
	__read_only __global Sphere* spheres,
	__read_only __global Plane* planes,
	__read_only __global float3* vertices,
	__read_only __global TriangleData* triangles,
	__read_only __global Material* materials,
	image2d_array_t textures,
	__read_only __global Light* lights) {
	//__read_only image2d_array_t textures) {
	int x = get_global_id(0);
	int y = get_global_id(1);

	Scene scene;
	//if (get_local_id(0) == 0 && get_local_id(1) == 0)
	{
		loadScene(
			inputData->numSpheres,
			spheres,
			inputData->numPlanes,
			planes,
			inputData->numVertices,
			vertices,
			inputData->numTriangles,
			triangles,
			materials,
			&textures,
			inputData->numLights,
			lights,
			&scene);
	}
	//barrier(CLK_LOCAL_MEM_FENCE);
	
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
		outColor += traceRay(&scene, &item.ray, item.multiplier, &stack);

		if (++iterCount >= MAX_ITERATIONS)
		{
			break;
		}
	}

	write_imagef(output, (int2)(x, y), (float4)(outColor, 1.0f));
}