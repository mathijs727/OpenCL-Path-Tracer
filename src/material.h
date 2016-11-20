#pragma once
#include <glm.hpp>
#include "types.h"
#include "light.h"

namespace raytracer {
class Scene;

struct Material
{
	enum class Type
	{
		Mirror,
		Diffuse,
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
			int x;
		} diffuse;
	};

	static Material Diffuse(const glm::vec3& colour) {
		Material result;
		result.type = Type::Diffuse;
		result.colour = colour;
		return result;
	}

	static Material Mirror(const glm::vec3& colour)
	{
		Material result;
		result.type = Type::Mirror;
		result.colour = colour;
		return result;
	}
};

glm::vec3 whittedShading(
	const Light& light,
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	const glm::vec3& lightDir,
	const Material& material,
	const Scene& scene);
}