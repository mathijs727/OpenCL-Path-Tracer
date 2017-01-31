#pragma once
#include "template/surface.h"
#include "types.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

namespace raytracer
{
	class HDRTexture
	{
	public:
		HDRTexture(const char* fileName, bool isLinear = false);
		~HDRTexture();

		int getId() const { return _tex_id; };

		static float* getData(int id) { return s_textures[id].get(); };
		//static size_t getTextureSize() { return HDR_TEXTURE_WIDTH * HDR_TEXTURE_HEIGHT * sizeof(float) * 3; }
		static size_t getNumUniqueSurfaces() { return s_textures.size(); };
	public:
		static const uint HDR_TEXTURE_WIDTH = 1024;
		static const uint HDR_TEXTURE_HEIGHT = 1024;
	private:
		static std::unordered_map<std::string, int> s_texturesMap;
		static std::vector<std::unique_ptr<float[]>> s_textures;

		int _tex_id;
	};
}