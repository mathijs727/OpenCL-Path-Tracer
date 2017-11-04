#pragma once

#include <glm/glm.hpp>
#include "template/includes.h"
#include "template/surface.h"
#include "types.h"
#include "texture.h"
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
			CL_VEC3(absorption);
			float smoothness;
			float refractiveIndex;
		} refractive;
		struct
		{
			CL_VEC3(absorption);
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
		result.refractive.absorption = glm::vec3(0);
		return result;
	}

	static Material Refractive(float smoothness, float refractiveIndex, glm::vec3 colour, float absorptionFactor)
	{
		Material result;
		result.type = Type::REFRACTIVE;
		result.refractive.smoothness = smoothness;
		result.refractive.refractiveIndex = refractiveIndex;
		result.refractive.absorption = (1.0f - colour) * absorptionFactor;
		return result;
	}

	static Material BasicRefractive(float refractiveIndex)
	{
		Material result;
		result.type = Type::BASIC_REFRACTIVE;
		result.basicRefractive.refractiveIndex = refractiveIndex;
		result.basicRefractive.absorption = glm::vec3(0);
		return result;
	}

	static Material BasicRefractive(float refractiveIndex, glm::vec3 colour, float absorptionFactor)
	{
		Material result;
		result.type = Type::BASIC_REFRACTIVE;
		result.basicRefractive.refractiveIndex = refractiveIndex;
		result.basicRefractive.absorption = (1.0f - colour) * absorptionFactor;
		return result;
	}

	static Material Emissive(const glm::vec3& colour, float intensityLumen = 500.0f)
	{
		// Cornell box is not defined in physical units so multiply by 5 to make it brighter
		Material result;
		result.type = Type::EMISSIVE;
		result.emissive.emissiveColour = colour * intensityLumen;
		return result;
	}

	static Material Emissive(float colourTemperature, float intensityLumen)
	{
		// http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
		// Approximate colour temp -> RGB
		colourTemperature /= 100;
		float red;
		if (colourTemperature <= 66) {
			red = 1.0f;
		} else {
			red = colourTemperature - 60;
			red = 329.698727446f * powf(red, -0.1332047592f);
			red /= 255.0f;
			red = std::min(1.0f, std::max(0.0f, red));
		}

		float green;
		if (colourTemperature <= 66) {
			green = colourTemperature;
			green = 99.4708025861f * log(green) - 161.1195681661f;
		} else {
			green = colourTemperature - 60;
			green = 288.1221695283f * powf(green, -0.0755148492f);
		}
		green /= 255.0f;
		green = std::min(1.0f, std::max(0.0f, green));

		float blue;
		if (colourTemperature >= 66) {
			blue = 1.0f;
		} else {
			if (colourTemperature <= 19) {
				blue = 0.0f;
			}
			else {
				blue = colourTemperature - 10;
				blue = 138.5177312231f * log(blue) - 305.0447927307f;
				blue /= 255.0f;
				blue = std::min(1.0f, std::max(0.0f, blue));
			}
		}

		glm::vec3 rgb = glm::vec3(red, green, blue);
		rgb = glm::pow(rgb, glm::vec3(2.2f));// Gamma corrected to linear
		return Emissive(rgb, intensityLumen);
	}
};
}