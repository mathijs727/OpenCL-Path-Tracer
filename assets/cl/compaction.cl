#include "prefix_sum.cl"
#include "shading.cl"

__kernel void reorderRays(
	__global int* orderBuffer,
	__global ShadingData* inDataBuffer,
	__global ShadingData* outDataBuffer)
{
	int gid = get_global_id(0);
	if (!(inDataBuffer[gid].flags & SHADINGFLAGS_HASFINISHED))
	{
		outDataBuffer[orderBuffer[gid]] = inDataBuffer[gid];
	}
}