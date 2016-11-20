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

	Type type;
	glm::vec3 colour;
	union
	{
		struct
		{
			i8 shit;
		} mirror;
		struct
		{
			i8 waddup;
		} diffuse;
	};

	static Material Diffuse(const glm::vec3& colour) {
		Material result;
		result.type = Type::Diffuse;
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