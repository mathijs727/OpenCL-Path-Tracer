#include "shapes.cl"
#include "rawshapes.cl"

__kernel void hello(
	__global float* out,
	uint width,// Render target width
	float3 eye,// Position of the camera "eye"
	float3 screen,// Left top of screen in world space
	float3 u_step,// Horizontal distance between pixels in world space
	float3 v_step,// Vertical distance between pixels in world space
	int num_spheres,// Scene
	__global RawSphere* spheres) {
	int x = get_global_id(0);
	int y = get_global_id(1);

	float3 screenPoint = screen + u_step * (float)x + v_step * (float)y;
	Ray ray;
	ray.origin = eye;
	ray.direction = normalize(screenPoint - eye);

	float3 outColor = (float3)(0.0f, 0.0f, 0.0f);
	float t = 0.0f;
	for (int i = 0; i < 1; i++)
	{
		RawSphere rawSphere = spheres[i];;
		Sphere sphere;
		convertRawShape(&rawSphere, &sphere);
		if (intersect(&ray, &sphere, &t))
			outColor.x = 1.0f;
	}

	// Use get_global_id instead of x/y to prevent casting from int to size_t
	size_t outIndex = y * width + x;
	out[outIndex * 3 + 0] = outColor.x;
	out[outIndex * 3 + 1] = outColor.y;
	out[outIndex * 3 + 2] = outColor.z;
}