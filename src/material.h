#pragma once
#include <glm.hpp>
#include "types.h"
#include "light.h"
#include <algorithm>

namespace raytracer {
class Scene;

struct Material
{
	enum class Type
	{
		Reflective,
		Diffuse,
		Glossy,
		Fresnel
	};

	Material() { }
	Material(const Material& m)
	{
		memcpy(this, &m, sizeof(Material));
	}
	Material& operator=(const Material& m)
	{
		memcpy(this, &m, sizeof(Material));
		return *this;
	}

	Type type;
	glm::vec3 colour;
	
	union
	{
		struct
		{
			float specularity;
		} glossy;
		struct
		{
			float boh;
		} fresnel;
	};

	static Material Diffuse(const glm::vec3& colour) {
		Material result;
		result.type = Type::Diffuse;
		result.colour = colour;
		return result;
	}

	static Material Reflective(const glm::vec3& colour)
	{
		Material result;
		result.type = Type::Reflective;
		result.colour = colour;
		return result;
	}

	static Material Glossy(const glm::vec3& colour, float specularity) {
		Material result;
		result.type = Type::Glossy;
		result.colour = colour;
		result.glossy.specularity = std::min(std::max(specularity, 0.f),1.f);
		return result;
	}
};

glm::vec3 whittedShading(
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	const Material& material,
	const Scene& scene);
}