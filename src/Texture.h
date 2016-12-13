#pragma once
#include "template/surface.h"
#include "types.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace raytracer
{
	class Texture
	{
	public:
		Texture(const char* fileName);
		~Texture();

		int getId() const { return _tex_id; };
		
		static Tmpl8::Surface* getSurface(int id) { return s_textures[id].get(); };
		static size_t getNumUniqueSurfaces() { return s_textures.size(); };
	public:
		static const uint TEXTURE_WIDTH = 1024;
		static const uint TEXTURE_HEIGHT = 1024;
	private:
		static std::unordered_map<const char*, int> s_texturesMap;
		static std::vector<std::unique_ptr<Tmpl8::Surface>> s_textures;

		int _tex_id;
	};
}