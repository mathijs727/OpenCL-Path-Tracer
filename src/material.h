#pragma once

#include <glm.hpp>
#include "types.h"

namespace raytracer {

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

}