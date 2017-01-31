#include "hdrtexture.h"
#include "types.h"
#include <FreeImage.h>
#include <vector>
#include <unordered_map>
#include <iostream>

std::unordered_map<std::string, int> raytracer::HDRTexture::s_texturesMap =
	std::unordered_map<std::string, int>();
std::vector<std::unique_ptr<float[]>> raytracer::HDRTexture::s_textures =
	std::vector<std::unique_ptr<float[]>>();

const uint raytracer::HDRTexture::HDR_TEXTURE_WIDTH;// = 1024;
const uint raytracer::HDRTexture::HDR_TEXTURE_HEIGHT;// = 1024;

raytracer::HDRTexture::HDRTexture(const char* fileName, bool isLinear)
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

		if (FreeImage_GetWidth(dib) != HDR_TEXTURE_WIDTH ||
			FreeImage_GetHeight(dib) != HDR_TEXTURE_HEIGHT)
		{
			dib = FreeImage_Rescale(dib, HDR_TEXTURE_WIDTH, HDR_TEXTURE_HEIGHT, FILTER_LANCZOS3);
		}

		if (!isLinear)
			FreeImage_AdjustGamma(dib, 1.0f / 2.2f);

		
		size_t memSize = HDR_TEXTURE_WIDTH * HDR_TEXTURE_HEIGHT * sizeof(float) * 4;
		s_textures.emplace_back(new float[HDR_TEXTURE_WIDTH * HDR_TEXTURE_HEIGHT * 4]);

		int w = FreeImage_GetWidth(dib);
		int h = FreeImage_GetHeight(dib);
		std::cout << "dims: (" << w << ", " << h << ")" << std::endl;
		
		// Store and add alpha channel
		//FreeImage_ConvertToRawBits((byte*)data, dib, FreeImage_GetPitch(dib), 4 * sizeof(float), FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, true);
		//memcpy(data, FreeImage_GetBits(dib), memSize);
		float* inData = (float*)FreeImage_GetBits(dib);
		float* outData = s_textures[texId].get();
		for (int y = 0; y < HDR_TEXTURE_HEIGHT; y++)
		{
			for (int x = 0; x < HDR_TEXTURE_WIDTH; x++)
			{
				outData[(y * HDR_TEXTURE_WIDTH + x) * 4 + 0] = inData[(y * HDR_TEXTURE_WIDTH + x) * 3 + 0];
				outData[(y * HDR_TEXTURE_WIDTH + x) * 4 + 1] = inData[(y * HDR_TEXTURE_WIDTH + x) * 3 + 1];
				outData[(y * HDR_TEXTURE_WIDTH + x) * 4 + 2] = inData[(y * HDR_TEXTURE_WIDTH + x) * 3 + 2];
				outData[(y * HDR_TEXTURE_WIDTH + x) * 4 + 3] = 1.0f;
			}
		}
		FreeImage_Unload(dib);

		//s_textures.push_back(std::make_unique<Tmpl8::Surface>(fileName, HDR_TEXTURE_WIDTH, HDR_TEXTURE_HEIGHT, isLinear));
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
