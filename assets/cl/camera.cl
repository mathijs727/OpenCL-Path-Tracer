#ifndef __CAMERA_CL
#define __CAMERA_CL

typedef struct
{
	// Pinhole camera
	float3 screenPoint;// Top left corner of screen
	float3 u;
	float3 v;

	// Physically based camera
	float3 eyePoint;
	float3 lookAtPoint;
	float3 u_normalized;
	float3 v_normalized;
	float apertureRadius;
} Camera;

Ray generateRay(__global Camera* camera, int x, int y, clrngMrg31k3pStream* randomStream)
{
	float width = get_global_size(0);
	float height = get_global_size(1);
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

// https://steveharveynz.wordpress.com/2012/12/21/ray-tracer-part-5-depth-of-field/
Ray generateRayDOF(__global Camera* camera, int x, int y, clrngMrg31k3pStream* randomStream)
{

}

#endif // __CAMERA_CL