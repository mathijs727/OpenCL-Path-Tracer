#include "shapes.cl"
#include "rawshapes.cl"
#include "material.cl"
#include "scene.cl"
#include "light.cl"

__kernel void hello(
	__global float* out,
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

	float3 screenPoint = screen + u_step * (float)x + v_step * (float)y;
	Ray ray;
	ray.origin = eye;
	ray.direction = normalize(screenPoint - eye);

	float3 outColor = traceRay(&l_scene, &ray);

	// Use get_global_id instead of x/y to prevent casting from int to size_t
	size_t outIndex = y * width + x;
	out[outIndex * 3 + 0] = outColor.x;
	out[outIndex * 3 + 1] = outColor.y;
	out[outIndex * 3 + 2] = outColor.z;
}