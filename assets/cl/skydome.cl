#ifndef __SKYDOME_CL
#define __SKYDOME_CL

__constant sampler_t skydomeSampler =
	CLK_NORMALIZED_COORDS_TRUE |
	CLK_ADDRESS_REPEAT |
	CLK_FILTER_LINEAR;


// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2009%20-%20various.pdf
// Slide 36
float3 readSkydome(float3 dir, image2d_array_t texture)
{
	// Convert unit vector to polar coordinates
	float u = 1 + atan2(dir.x, -dir.z) / PI;
	float v = acos(dir.y) / PI;

	// Outputted u is in the range [0, 2], we sample using normalized coordinates [0, 1]
	u /= 2;

	float4 colourWithAlpha = read_imagef(
		texture,
		skydomeSampler,
		(float4)(u, 1.0f - v, 0, 0));
	return colourWithAlpha.xyz;
}

#endif // __SKYDOME_CL
