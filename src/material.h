#pragma once

#include <glm.hpp>
#include "template/includes.h"
#include "template/surface.h"
#include "types.h"
#include "Texture.h"
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

#define CL_VEC3(NAME) \
	union { \
		glm::vec3 NAME; \
		cl_float3 __cl_padding; \
	}

namespace raytracer {
class Scene;

struct Material
{
	enum class Type : cl_int
	{
		Diffuse,
		Emissive
	};

	Material() { }
	Material(const Material& m) { memcpy(this, &m, sizeof(Material)); }
	Material& operator=(const Material& m) { memcpy(this, &m, sizeof(Material)); return *this; }
	
	union
	{
		struct
		{
			CL_VEC3(diffuseColour);
			cl_int tex_id; byte __padding[12];
		} diffuse;
		struct
		{
			CL_VEC3(emissiveColour);
		} emissive;
	};
	Type type; // 4 bytes
	byte __padding2[12];

	static Material Diffuse(const glm::vec3& colour) {
		Material result;
		result.type = Type::Diffuse;
		result.diffuse.diffuseColour = colour;
		result.diffuse.tex_id = -1;
		return result;
	}

	static Material Diffuse(const raytracer::Texture& diffuse, const glm::vec3& colour = glm::vec3(0)) {
		Material result;
		result.type = Type::Diffuse;
		result.diffuse.diffuseColour = colour;
		result.diffuse.tex_id = diffuse.getId();
		return result;
	}

	static Material Emissive(const glm::vec3& colour) {
		Material result;
		result.type = Type::Emissive;
		result.emissive.emissiveColour = colour;
		return result;
	}

	/*static Material Reflective(const glm::vec3& colour) {
		Material result;
		result.type = Type::Reflective;
		result.colour = colour;
		return result;
	}

	static Material Glossy(const glm::vec3& colour, float specularity) {
		Material result;
		result.type = Type::Glossy;
		result.colour = colour;
		result.glossy.specularity = std::min(std::max(specularity, 0.0f), 1.0f);
		return result;
	}

	static Material Fresnel(const glm::vec3& colour, float refractive_index, glm::vec3 absorption) {
		Material result;
		result.type = Type::Fresnel;
		result.colour = colour;
		result.fresnel.refractive_index = std::max(refractive_index, 1.f);
		result.fresnel.absorption = absorption;
		return result;
	}*/
};
}