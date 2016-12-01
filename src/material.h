#pragma once

#include <glm.hpp>
#include "template/includes.h"
#include "template/surface.h"
#include "types.h"
#include "light.h"
#include <algorithm>
#include <vector>
#include <iostream>
#include <unordered_map>

#define RAYTRACER_EPSILON 0.0001f

namespace Tmpl8
{
	class Surface;
}

namespace raytracer {
class Scene;

struct Material
{
	static const uint TEXTURE_WIDTH = 1024;
	static const uint TEXTURE_HEIGHT = 1024;
	static std::unordered_map<Tmpl8::Surface*, int> s_texturesMap;
	static std::vector<Tmpl8::Surface*> s_textures;

	enum class Type : cl_int
	{
		Reflective,
		Diffuse,
		Glossy,
		Fresnel
	};

	Material() { }
	Material(const Material& m) { memcpy(this, &m, sizeof(Material)); }
	Material& operator=(const Material& m) { memcpy(this, &m, sizeof(Material)); return *this; }
	
	union { //16 bytes
		glm::vec3 colour;
		cl_float3 __cl_colour;
	};

	// 16 bytes total
	Type type; // 4 bytes
	union // 4 bytes
	{
		struct
		{
			cl_int tex_id;
		} diffuse;
		struct
		{
			cl_float specularity;
		} glossy;
		struct
		{
			cl_float refractive_index;
		} fresnel;
	};
	byte __padding[8];// 8 bytes

	static Material Diffuse(const glm::vec3& colour) {
		Material result;
		result.type = Type::Diffuse;
		result.colour = colour;
		result.diffuse.tex_id = -1;
		return result;
	}

	static Material Diffuse(Tmpl8::Surface* diffuse) {
		cl_int tex_id;
		auto res = s_texturesMap.find(diffuse);
		if (res == s_texturesMap.end())
		{
			Tmpl8::Surface* texture;
			if (diffuse->GetWidth() != TEXTURE_WIDTH ||
				diffuse->GetHeight() != TEXTURE_HEIGHT)
			{
				if (diffuse->m_fileName != nullptr)
				{
					texture = new Tmpl8::Surface(diffuse->m_fileName, TEXTURE_WIDTH, TEXTURE_HEIGHT);
				}
				else {
					std::cout << "ERROR: Texture should be of size: (" << TEXTURE_WIDTH << ", " << TEXTURE_HEIGHT << ")" << std::endl;
					exit(EXIT_FAILURE);
				}
			}
			else {
				texture = diffuse;
			}

			tex_id = static_cast<cl_int>(s_textures.size());
			s_textures.push_back(texture);
			s_texturesMap.insert(std::pair<Tmpl8::Surface*, int>(diffuse, tex_id));
		}
		else {
			tex_id = res->second;
		}

		Material result;
		result.type = Type::Diffuse;
		result.colour = glm::vec3(0);
		result.diffuse.tex_id = tex_id;
		return result;
	}

	static Material Reflective(const glm::vec3& colour)	{
		Material result;
		result.type = Type::Reflective;
		result.colour = colour;
		return result;
	}

	static Material Glossy(const glm::vec3& colour, float specularity) {
		Material result;
		result.type = Type::Glossy;
		result.colour = colour;
		result.glossy.specularity = std::min(std::max(specularity, 0.0f),1.0f);
		return result;
	}

	static Material Fresnel(const glm::vec3& colour, float refractive_index) {
		Material result;
		result.type = Type::Fresnel;
		result.colour = colour;
		result.fresnel.refractive_index = std::max(refractive_index, 1.f);
		return result;
	}
};

struct SerializedMaterial
{
	Material::Type type;
	glm::vec3 colour;

	union
	{
		struct
		{
			float specularity;
		} glossy;
		struct
		{
			float refrective_index;
		} fresnel;
	};

	SerializedMaterial() { }
	SerializedMaterial(const Material& other)
	{
		type = other.type;
		colour = other.colour;

		if (other.type == Material::Type::Fresnel)
		{
			fresnel.refrective_index = other.fresnel.refractive_index;
		}
		else if (other.type == Material::Type::Glossy)
		{
			glossy.specularity = other.glossy.specularity;
		}
	}
};
}