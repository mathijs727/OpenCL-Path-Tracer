#ifndef __CUBEMAP_CL
#define __CUBEMAP_CL
#include "scene.cl"

enum
{
	CUBEMAP_TOP = 0,
	CUBEMAP_BOTTOM = 1,
	CUBEMAP_LEFT = 2,
	CUBEMAP_RIGHT = 3,
	CUBEMAP_FRONT = 4,
	CUBEMAP_BACK = 5
};

__constant sampler_t cubemapSampler =
	CLK_NORMALIZED_COORDS_TRUE |
	CLK_ADDRESS_REPEAT |
	CLK_FILTER_LINEAR;


float3 sampleTex(image2d_array_t textures, float x, float y, int texId)
{
	float4 texCoords3d = (float4)(x, y, texId, 0.0f);
	float4 colourWithAlpha = read_imagef(
		textures,
		cubemapSampler,
		texCoords3d);
	return colourWithAlpha.xyz;
}

// https://www.ics.uci.edu/~gopi/CS211B/RayTracing%20tutorial.pdf
// Page 20
float3 readCubeMap(float3 dir, image2d_array_t textures, __local Scene* scene)
{
	float3 c = (float3)(0,0,0);

	if (fabs(dir.x) >= fabs(dir.y) &&
		fabs(dir.x) >= fabs(dir.z))
	{
		if (dir.x < 0.0f)
		{
			// Right
			c = sampleTex(textures,
				1.0f - (dir.y / dir.x + 1.0f) * 0.5f,
				(dir.z / dir.x + 1.0f) * 0.5f,
				CUBEMAP_RIGHT);
		} else if (dir.x > 0.0f) {
			// Left
			c = sampleTex(textures,
				(dir.y / dir.x + 1.0f) * 0.5f,
				(dir.z / dir.x + 1.0f) * 0.5f,
				CUBEMAP_LEFT);
		}
	} else if (fabs(dir.y) >= fabs(dir.x)  && fabs(dir.y) >= fabs(dir.z))
	{
		if (dir.y > 0.0f)
		{
			// Top
			c = sampleTex(textures,
				(dir.z / dir.y + 1.0f) * 0.5f,
				(dir.x / dir.y + 1.0f) * 0.5f,
				CUBEMAP_TOP);
		} else if (dir.y < 0.0f)
		{
			// Bottom
			c = sampleTex(textures,
				(dir.z / dir.y + 1.0f) * 0.5f,
				1.0f - (dir.x / dir.y + 1.0f) * 0.5f,
				CUBEMAP_BOTTOM);
		}
	} else if (fabs(dir.z) >= fabs(dir.x) && fabs(dir.z) >= fabs(dir.y))
	{
		if (dir.z < 0.0f)
		{
			// Front
			c = sampleTex(textures,
				1.0f - (dir.y / dir.z + 1.0f) * 0.5f,
				1.0f - (dir.x / dir.z + 1.0f) * 0.5f,
				CUBEMAP_FRONT);
		} else if (dir.z > 0.0f)
		{
			// Back
			c = sampleTex(textures,
				1.0f - (dir.y / dir.z + 1.0f) * 0.5f,
				(dir.x / dir.z + 1.0f) * 0.5f,
				CUBEMAP_BACK);
		}
	}

	return c;
}

#endif // __CUBEMAP_CL
