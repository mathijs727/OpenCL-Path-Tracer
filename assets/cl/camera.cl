#ifndef __CAMERA_CL
#define __CAMERA_CL

typedef struct
{
	// Pinhole camera
	float3 eyePoint;
	float3 screenPoint;// Top left corner of screen
	float3 u;
	float3 v;

	// Physically based camera
	float3 u_normalized;
	float3 v_normalized;
	float focalDistance;
	float apertureRadius;
} Camera;

Ray generateRayPinhole(
	volatile __global Camera* camera,
	int x,
	int y,
	float width,
	float height,
	clrngMrg31k3pStream* randomStream)
{
	//float width = get_global_size(0);
	//float height = get_global_size(1);
	float3 u_step = camera->u / width;
	float3 v_step = camera->v / height;
	float3 screenPoint = camera->screenPoint + \
		u_step * x + v_step * y;	
	screenPoint += (float)clrngMrg31k3pRandomU01(randomStream) * u_step;
	screenPoint += (float)clrngMrg31k3pRandomU01(randomStream) * v_step;

	Ray result;
	result.origin = camera->eyePoint;
	result.direction = normalize(screenPoint - camera->eyePoint);
	return result;
}

/*float length(float3 vec)
{
	return sqrt(dot(vec, vec));
}*/

// http://http.developer.nvidia.com/GPUGems/gpugems_ch23.html
// https://courses.cs.washington.edu/courses/cse457/99sp/projects/trace/depthoffield.doc
Ray generateRayThinLens(
	volatile __global Camera* camera,
	int x,
	int y,
	float width,
	float height,
	clrngMrg31k3pStream* randomStream)
{
	float r1 = (float)clrngMrg31k3pRandomU01(randomStream) * 2.0f - 1.0f;
	float r2 = (float)clrngMrg31k3pRandomU01(randomStream) * 2.0f - 1.0f;
	// TODO: Round aperture
	float3 offsetOnLense = r1 * camera->u_normalized * camera->apertureRadius + \
						   r2 * camera->v_normalized * camera->apertureRadius;

	Ray primaryRay = generateRayPinhole(camera, x, y, width, height, randomStream);
	float3 focalPoint = primaryRay.origin + camera->focalDistance * primaryRay.direction;
	float3 pointOnLens = primaryRay.origin + offsetOnLense;
	float3 direction = focalPoint - pointOnLens;

	Ray secondaryRay;
	secondaryRay.origin = pointOnLens;
	secondaryRay.direction = direction;
	return secondaryRay;
}

#endif // __CAMERA_CL