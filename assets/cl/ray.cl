#ifndef __RAY_CL
#define __RAY_CL
typedef struct
{
	float3 origin;
	float3 direction;
} Ray;

Ray createRay(float3 origin, float3 direction)
{
	Ray ray;
	ray.origin = origin;
	ray.direction = direction;
	return ray;
}
#endif// __RAY_CL