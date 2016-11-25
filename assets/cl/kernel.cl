#include "shapes.cl"
#include "rawshapes.cl"
#include "material.cl"

typedef enum
{
	SphereType,
	PlaneType
} IntersectionType;

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
	__global RawMaterial* materials) {
	int x = get_global_id(0);
	int y = get_global_id(1);

	float3 screenPoint = screen + u_step * (float)x + v_step * (float)y;
	Ray ray;
	ray.origin = eye;
	ray.direction = normalize(screenPoint - eye);

	float minT = 100000.0f;
	int i_current_hit = -1;
	IntersectionType type;
	for (int i = 0; i < numSpheres; i++)
	{
		RawSphere rawSphere = spheres[i];
		Sphere sphere;
		convertRawSphere(&rawSphere, &sphere);
		float t;
		if (intersectSphere(&ray, &sphere, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = SphereType;
		}
	}

	for (int i = 0; i < numPlanes; i++)
	{
		RawPlane rawPlane = planes[i];
		Plane plane;
		convertRawPlane(&rawPlane, &plane);
		float t;
		if (intersectPlane(&ray, &plane, &t) && t < minT)
		{
			minT = t;
			i_current_hit = i;
			type = PlaneType;
		}
	}

	Material material;
	if (type == SphereType)
	{
		RawMaterial rawMaterial = materials[i_current_hit];
		convertRawMaterial(&rawMaterial, &material);
	} else if (type = PlaneType)
	{
		RawMaterial rawMaterial = materials[numSpheres + i_current_hit];
		convertRawMaterial(&rawMaterial, &material);
	}
	float3 outColor = material.colour;

	// Use get_global_id instead of x/y to prevent casting from int to size_t
	size_t outIndex = y * width + x;
	out[outIndex * 3 + 0] = outColor.x;
	out[outIndex * 3 + 1] = outColor.y;
	out[outIndex * 3 + 2] = outColor.z;
}