#ifndef __LIGHT_CL
#define __LIGHT_CL
#include "material.cl"

typedef struct
{
	float3 vertices[3];
	Material material;
} EmisiveTriangle;

#endif// __LIGHT_CL