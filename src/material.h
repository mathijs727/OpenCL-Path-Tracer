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
		DIFFUSE,
		PBR,
		REFRACTIVE,
		BASIC_REFRACTIVE,
		EMISSIVE
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
			// Metallic is a boolean flag
			// Applied as "Hodgman" suggests (not well documented in any PBR papers from Unreal or Dice):
			// https://www.gamedev.net/topic/672836-pbr-metalness-equation/
			union
			{
				CL_VEC3(baseColour);// Non metal
				CL_VEC3(reflectance);// Metal (f0 for Schlick approximation of Fresnel)
			};
			float smoothness;
			float f0NonMetal;
			bool metallic; byte __padding[4];
		} pbr;
		struct
		{
			float f0;
			float smoothness;
			float refractiveIndex;
		} refractive;
		struct
		{
			float refractiveIndex;
		} basicRefractive;
		struct
		{
			CL_VEC3(emissiveColour);
		} emissive;
	};
	Type type; // 4 bytes
	byte __padding2[12];

	static Material Diffuse(const glm::vec3& colour) {
		Material result;
		result.type = Type::DIFFUSE;
		result.diffuse.diffuseColour = colour;
		result.diffuse.tex_id = -1;
		return result;
	}

	static Material Diffuse(const raytracer::Texture& diffuse, const glm::vec3& colour = glm::vec3(0)) {
		Material result;
		result.type = Type::DIFFUSE;
		result.diffuse.diffuseColour = colour;
		result.diffuse.tex_id = diffuse.getId();
		return result;
	}

	static Material PBRMetal(const glm::vec3 reflectance, float smoothness)
	{
		Material result;
		result.type = Type::PBR;
		result.pbr.reflectance = reflectance;
		result.pbr.smoothness = smoothness;
		result.pbr.metallic = true;
		return result;
	}

	static Material PBRDielectric(const glm::vec3 baseColour, float smoothness, float f0 = 0.04f)
	{
		Material result;
		result.type = Type::PBR;
		result.pbr.baseColour = baseColour;
		result.pbr.smoothness = smoothness;
		result.pbr.f0NonMetal = f0;
		result.pbr.metallic = false;
		return result;
	}

	static Material Refractive(float smoothness, float refractiveIndex)
	{
		Material result;
		result.type = Type::REFRACTIVE;
		result.refractive.smoothness = smoothness;
		result.refractive.refractiveIndex = refractiveIndex;
		return result;
	}

	static Material BasicRefractive(float refractiveIndex)
	{
		Material result;
		result.type = Type::BASIC_REFRACTIVE;
		result.basicRefractive.refractiveIndex = refractiveIndex;
		return result;
	}

	static Material Emissive(const glm::vec3& colour) {
		// Cornell box is not defined in physical units so multiply by 5 to make it brighter
		Material result;
		result.type = Type::EMISSIVE;
		result.emissive.emissiveColour = colour * 10.0f;
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