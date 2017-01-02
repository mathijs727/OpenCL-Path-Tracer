__constant sampler_t sampler =
	CLK_NORMALIZED_COORDS_FALSE |
	CLK_ADDRESS_NONE |
	CLK_FILTER_NEAREST;

__kernel void accumulate(
	__read_only image2d_t input,
	__write_only image2d_t output,
	int n)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int2 texCoords = (int2)(x, y);
	float nf = (float)n;

	// Read the sum of the rays and divide by number of rays
	float4 raySum = read_imagef(input, sampler, texCoords);
	float4 colour = raySum / nf;

	// Output average colour
	write_imagef(output, texCoords, colour);
}