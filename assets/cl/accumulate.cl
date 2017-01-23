#include "gamma.cl"
#include "exposure.cl"
#include "tonemapping.cl"
#include "kernel_data.cl"

__kernel void accumulate(
	__write_only image2d_t output,
	__global float3* input,

	__global const KernelData* inputData,
	uint n,
	uint scr_width)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int2 texCoords = (int2)(x, y);
	float nf = (float)n;

	// Read the sum of the rays and divide by number of rays
	float3 raySum = input[y * scr_width + x];
	float3 luminance = raySum / nf;

	// Adjust for exposure
	luminance = calcExposedLuminance(&inputData->camera, luminance);

	// Apply tone mapping
	float3 colour = tonemapUncharted2(luminance);

	// Gamma correct the colour
	colour = accurateLinearToSRGB(colour);

	// Output average colour
	write_imagef(output, texCoords, (float4)(colour, 1.0f));
}