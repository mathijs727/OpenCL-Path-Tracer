#pragma once
#include <glm.hpp>
#include "ray.h"

namespace raytracer
{
	class Scene;

	struct Light
	{
		enum class Type
		{
			Point,
			Directional
		};

		Type type;
		glm::vec3 colour;
		union
		{
			struct
			{
				glm::vec3 position;
			} point;
			struct
			{
				glm::vec3 direction;
			} directional;
		};

		// Apperently necesarry (maybe got something to do with glm::vec in union???)
		Light() {};
		Light(const Light& other) {
			type = other.type;
			colour = other.colour;
			if (type == Type::Point)
			{
				point.position = other.point.position;
			}
			else if (type == Type::Directional)
			{
				directional.direction = other.directional.direction;
			}
		};


		static Light Point(const glm::vec3& colour, const glm::vec3 pos)
		{
			Light light;
			light.type = Type::Point;
			light.colour = colour;
			light.point.position = pos;
			return light;
		}

		static Light Directional(const glm::vec3& colour, const glm::vec3 dir)
		{
			Light light;
			light.type = Type::Directional;
			light.colour = colour;
			light.directional.direction = glm::normalize(dir);
			return light;
		}
	};

	bool get_light_vector(const Light& light, const glm::vec3 position, glm::vec3& lightDirection);
	bool is_light_visible(const glm::vec3& position, const Light& light, const Scene& scene);
	glm::vec3 trace_light(const glm::vec3& position, const Light& light, const Scene& scene);
}