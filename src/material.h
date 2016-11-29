#pragma once

#include <glm.hpp>
#include "template/includes.h"
#include "types.h"
#include "light.h"
#include <algorithm>

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
	byte __padding[8];

	static Material Diffuse(const glm::vec3& colour) {
		Material result;
		result.type = Type::Diffuse;
		result.colour = colour;
		//result.diffuse.diffuse_texture = nullptr;
		return result;
	}

	/*static Material Diffuse(Tmpl8::Surface* diffuse) {
		Material result;
		result.type = Type::Diffuse;
		result.colour = glm::vec3(0);
		result.diffuse.diffuse_texture = diffuse;
		return result;
	}*/

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