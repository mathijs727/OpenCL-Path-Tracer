// This file is based on AMD open source ray tracing software: FireRays 2.0:
// https://github.com/GPUOpen-LibrariesAndSDKs/RadeonRays_SDK/blob/master/App/CL/texture.cl
#ifndef __TEXTURE_CL
#define __TEXTURE_CL
#include "utils.cl"

enum {
	IMAGETYPE_BGRA8 = 0,
};

typedef struct
{
	uint width;
	uint height;
	uint dataOffset;
	int imageType;

} ImageDescriptor;

typedef struct
{
	__global char* data;
	__global ImageDescriptor* descriptors;
} Textures;

Textures loadTextures(
	__global char* data,
	__global ImageDescriptor* descriptors)
{
	Textures result;
	result.data = data;
	result.descriptors = descriptors;
	return result;
}

// Normalized coordinates (x and y between 0 and 1)
float4 sample2D(const Textures* textures, int texId, float2 texCoords)
{
	ImageDescriptor descriptor = textures->descriptors[texId];
	int width = descriptor.width;
	int height = descriptor.height;

	int x0 = (int)(width * texCoords.x);
	int y0 = (int)(height * texCoords.y);
	int x1 = x0 + 1;
	int y1 = y0 + 1;


	// Out of bounds indexing = repeating texture
	x0 = x0 % width;
	x1 = x1 % width;
	y0 = y0 % height;
	y1 = y1 % height;
	//return (float4)((float)x0 / width, (float)y0 / height, 0, 0);

	// Calculate weights for linear filtering
	float wx = texCoords.x * width - floor(texCoords.x * width);
	float wy = texCoords.y * height - floor(texCoords.y * height);

	__global char* mydata = textures->data + descriptor.dataOffset;
	if (descriptor.imageType == IMAGETYPE_BGRA8) {
		__global uchar4* mydatac = (__global uchar4*)mydata;

		// Get 4 values and convert to float
		uchar4 valu00 = *(mydatac + width * y0 + x0);
		uchar4 valu01 = *(mydatac + width * y0 + x1);
		uchar4 valu10 = *(mydatac + width * y1 + x0);
		uchar4 valu11 = *(mydatac + width * y1 + x1);

		float4 val00 = make_float4((float)valu00.z / 255.f, (float)valu00.y / 255.f, (float)valu00.x / 255.f, (float)valu00.w / 255.f);
		float4 val01 = make_float4((float)valu01.z / 255.f, (float)valu01.y / 255.f, (float)valu01.x / 255.f, (float)valu01.w / 255.f);
		float4 val10 = make_float4((float)valu10.z / 255.f, (float)valu10.y / 255.f, (float)valu10.x / 255.f, (float)valu10.w / 255.f);
		float4 val11 = make_float4((float)valu11.z / 255.f, (float)valu11.y / 255.f, (float)valu11.x / 255.f, (float)valu11.w / 255.f);

		// Filter and return the result
		return lerp(lerp(val00, val01, wx), lerp(val10, val11, wx), wy);
	} else {
		return (float4)(0, 0, 0, 0);
	}
}



#endif // __TEXTURE_CL