#include "texture.h"
#include "template/surface.h"
#include "types.h"
#include <vector>
#include <unordered_map>
#include <iostream>

std::unordered_map<std::string, int> raytracer::Texture::s_texturesMap =
	std::unordered_map<std::string, int>();
std::vector<std::unique_ptr<Tmpl8::Surface>> raytracer::Texture::s_textures =
	std::vector<std::unique_ptr<Tmpl8::Surface>>();

const uint raytracer::Texture::TEXTURE_WIDTH;// = 1024;
const uint raytracer::Texture::TEXTURE_HEIGHT;// = 1024;

raytracer::Texture::Texture(const char* fileName, bool isLinear)
{
	int texId;
	auto res = s_texturesMap.find(fileName);
	if (res == s_texturesMap.end())
	{
		texId = static_cast<int>(s_textures.size());
		std::cout << "Loading texture: " << fileName << std::endl;
		s_textures.push_back(std::make_unique<Tmpl8::Surface>(fileName, TEXTURE_WIDTH, TEXTURE_HEIGHT, isLinear));
		s_texturesMap.insert(std::pair<const char*, int>(fileName, texId));
	}
	else {
		//std::cout << "Reusing texture: " << fileName << std::endl;
		texId = res->second;
	}

	_tex_id = texId;
}

raytracer::Texture::~Texture()
{
}
