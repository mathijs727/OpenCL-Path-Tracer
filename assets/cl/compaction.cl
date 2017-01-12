#include "prefix_sum.cl"
#include "shading.cl"

__kernel void reorderRays(
	__global int* orderBuffer,
	__global ShadingData* inDataBuffer,
	__global ShadingData* outDataBuffer)
{
	int gid = get_global_id(0);
	outDataBuffer[gid] = inDataBuffer[orderBuffer[gid]];
}