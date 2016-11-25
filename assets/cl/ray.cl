#ifndef __RAY_CL
#define __RAY_CL
typedef struct
{
	float3 origin;
	float3 direction;
} Ray;

typedef struct
{
	float3 origin;
	float3 dest;
} Line;
#endif// __RAY_CL