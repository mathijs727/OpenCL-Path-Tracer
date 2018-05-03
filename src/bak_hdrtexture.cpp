#include "hdrtexture.h"

#ifdef __linux__
// https://stackoverflow.com/questions/7047013/using-c-with-objective-c-how-can-i-fix-conflicting-declaration-typedef-int
// FreeImage contains typedefs for BYTE and BOOL which are also in X11
#include <FreeImage.h>
#ifndef CARD8
#define BYTE CARD8
#define BOOL CARD8
#endif
#else
#include <freeimage.h>
#endif

#include "types.h"
#include <vector>
#include <unordered_map>
#include <iostream>

std::unordered_map<std::string, int> raytracer::HDRTexture::s_texturesMap =
	std::unordered_map<std::string, int>();
std::vector<std::unique_ptr<float[]>> raytracer::HDRTexture::s_textures =
	std::vector<std::unique_ptr<float[]>>();

//const uint raytracer::HDRTexture::HDR_TEXTURE_WIDTH;// = 1024;
//const uint raytracer::HDRTexture::HDR_TEXTURE_HEIGHT;// = 1024;

raytracer::HDRTexture::HDRTexture(const char* fileName, bool isLinear, float multiplier)
{
	int texId;
	auto res = s_texturesMap.find(fileName);
	if (res == s_texturesMap.end())
	{
		texId = static_cast<int>(s_textures.size());

		std::cout << "Loading HDR texture: " << fileName << std::endl;

		FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
		fif = FreeImage_GetFileType(fileName);
		if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(fileName);
		FIBITMAP* tmp = FreeImage_Load(fif, fileName);
		FIBITMAP* dib = FreeImage_ConvertToRGBF(tmp);
		FreeImage_Unload(tmp);

		/*if (FreeImage_GetWidth(dib) != HDR_TEXTURE_WIDTH ||
			FreeImage_GetHeight(dib) != HDR_TEXTURE_HEIGHT)
		{
			dib = FreeImage_Rescale(dib, HDR_TEXTURE_WIDTH, HDR_TEXTURE_HEIGHT, FILTER_LANCZOS3);
		}*/
		_width = FreeImage_GetWidth(dib);
		_height = FreeImage_GetHeight(dib);

		if (!isLinear)
			FreeImage_AdjustGamma(dib, 1.0f / 2.2f);

		
		size_t memSize = _width * _height * sizeof(float) * 4;
		s_textures.emplace_back(new float[_width * _height * 4]);

		int w = FreeImage_GetWidth(dib);
		int h = FreeImage_GetHeight(dib);
		std::cout << "dims: (" << w << ", " << h << ")" << std::endl;
		
		// Store and add alpha channel
		// We need to do this because OpenCL (at least on AMD) does not support RGB float textures, just RGBA float textures
		float* inData = (float*)FreeImage_GetBits(dib);
		float* outData = s_textures[texId].get();
		for (int y = 0; y < _height; y++)
		{
			for (int x = 0; x < _width; x++)
			{
				outData[(y * _width + x) * 4 + 0] = inData[(y * _width + x) * 3 + 0] * multiplier;
				outData[(y * _width + x) * 4 + 1] = inData[(y * _width + x) * 3 + 1] * multiplier;
				outData[(y * _width + x) * 4 + 2] = inData[(y * _width + x) * 3 + 2] * multiplier;
				outData[(y * _width + x) * 4 + 3] = 1.0f;
			}
		}
		FreeImage_Unload(dib);

		s_texturesMap.insert(std::pair<const char*, int>(fileName, texId));
	}
	else {
		texId = res->second;
	}

	_tex_id = texId;
}

raytracer::HDRTexture::~HDRTexture()
{
}
