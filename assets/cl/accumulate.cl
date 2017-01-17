#include "gamma.cl"

__kernel void accumulate(
	__global float3* input,
	__write_only image2d_t output,
	uint n,
	uint scr_width)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int2 texCoords = (int2)(x, y);
	float nf = (float)n;

	// Read the sum of the rays and divide by number of rays
	float3 raySum = input[y * scr_width + x];
	float3 colour = raySum / nf;

	// Gamma correct the colour
	colour = accurateLinearToSRGB(colour);

	// Output average colour
	write_imagef(output, texCoords, (float4)(colour, 1.0f));
}