#include "shapes.cl"
#include "rawshapes.cl"
#include "material.cl"
#include "scene.cl"
#include "light.cl"
#include "stack.cl"

__kernel void hello(
	__write_only image2d_t output,
	uint width,// Render target width
	float3 eye,// Position of the camera "eye"
	float3 screen,// Left top of screen in world space
	float3 u_step,// Horizontal distance between pixels in world space
	float3 v_step,// Vertical distance between pixels in world space
	int numSpheres,// Scene
	__global RawSphere* spheres,
	int numPlanes,
	__global RawPlane* planes,
	__global RawMaterial* materials,
	int numLights,
	__global RawLight* lights) {
	int x = get_global_id(0);
	int y = get_global_id(1);


	__local Scene l_scene;
	if (get_local_id(0) == 0 && get_local_id(1) == 0)
	{
		loadScene(numSpheres, spheres, numPlanes, planes, materials, numLights, lights, &l_scene);
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	float3 screenPoint = screen + u_step * (float)x + v_step * (float)y;	
	StackItem item;
	item.ray.origin = eye;
	item.ray.direction = normalize(screenPoint - eye);
	item.multiplier = (float3)(1.0f, 1.0f, 1.0f);

	float3 outColor = (float3)(0.0f, 0.0f, 0.0f);
	Stack stack;
	StackInit(&stack);
	StackPush(&stack, &item);
	while (!StackEmpty(&stack))
	{
		StackItem item;
		StackPop(&stack, &item);
		outColor += traceRay(&l_scene, &item.ray, item.multiplier, &stack);
	}

	float4 xxx;
	xxx.x = outColor.x;
	xxx.y = outColor.y;
	xxx.z = outColor.z;
	xxx.w = 1.0f;
	// Use get_global_id instead of x/y to prevent casting from int to size_t
	/*size_t outIndex = y * width + x;
	out[outIndex * 3 + 0] = outColor.x;
	out[outIndex * 3 + 1] = outColor.y;
	out[outIndex * 3 + 2] = outColor.z;*/
	write_imagef(output, (int2)(x, y), xxx);
}