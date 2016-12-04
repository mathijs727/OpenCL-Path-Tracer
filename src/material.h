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

namespace raytracer {
class Scene;

struct Material
{
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
	
	union
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
			union { //16 bytes
				glm::vec3 absorption;
				cl_float3 __cl_absorption;
			};
			cl_float refractive_index;// 4 bytes
			byte __padding1[12];
		} fresnel;
	};
	Type type; // 4 bytes
	byte __padding2[12];

	static Material Diffuse(const glm::vec3& colour) {
		Material result;
		result.type = Type::Diffuse;
		result.colour = colour;
		result.diffuse.tex_id = -1;
		return result;
	}

	static Material Diffuse(const raytracer::Texture& diffuse) {
		Material result;
		result.type = Type::Diffuse;
		result.colour = glm::vec3(0);
		result.diffuse.tex_id = diffuse.getId();
		return result;
	}

	static Material Reflective(const glm::vec3& colour) {
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
	}
};
}