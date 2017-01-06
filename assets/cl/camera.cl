#ifndef __CAMERA_CL
#define __CAMERA_CL

typedef struct
{
	/*float3 eye;// Position of the camera "eye"
	float3 screen;// Left top of screen in world space
	float3 u_step;// Horizontal distance between pixels in world space
	float3 v_step;// Vertical distance between pixels in world space
	uint width;// Render target width*/
	float3 eyePoint;
	float3 lookAtPoint;
	float3 u;
	float3 v;
	float apertureRadius;
} Camera;

Ray generateRay(__global Camera* camera, int x, int y, clrngMrg31k3pStream* randomStream)
{
	float3 screenPoint = camera->screen + \
		camera->u_step * (float)x + camera->v_step * (float)y;	
	screenPoint += (float)clrngMrg31k3pRandomU01(randomStream) * camera->u_step;
	screenPoint += (float)clrngMrg31k3pRandomU01(randomStream) * camera->v_step;

	Ray result;
	result.origin = camera->eye;
	result.direction = normalize(screenPoint - camera->eye);
	return result;
}

// https://steveharveynz.wordpress.com/2012/12/21/ray-tracer-part-5-depth-of-field/
Ray generateRayDOF(__global Camera* camera, int x, int y, clrngMrg31k3pStream* randomStream)
{

}

#endif // __CAMERA_CL