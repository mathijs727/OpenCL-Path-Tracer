#pragma once
#include <glm.hpp>
#include <string.h>
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
				float luminous_power;// In lumen
				glm::vec3 position;
			} point;
			struct
			{
				float illuminance;// In Lux
				glm::vec3 direction;
			} directional;
		};

		// Apperently necesarry (maybe got something to do with glm::vec in union???)
		Light() {};
		Light(const Light& other) {
			memcpy(this, &other, sizeof(Light));
		};


		static Light Point(const glm::vec3 pos, const glm::vec3& colour, float luminous_power)
		{
			Light light;
			light.type = Type::Point;
			light.colour = colour;
			light.point.position = pos;
			light.point.luminous_power = luminous_power;
			return light;
		}

		static Light Directional(const glm::vec3 dir, const glm::vec3& colour, float illuminance)
		{
			Light light;
			light.type = Type::Directional;
			light.colour = colour;
			light.directional.direction = glm::normalize(dir);
			light.directional.illuminance = illuminance;
			return light;
		}
	};

	bool get_light_vector(const Light& light, const glm::vec3 position, glm::vec3& lightDirection);
	bool is_light_visible(const glm::vec3& position, const Light& light, const Scene& scene);
	glm::vec3 trace_light(const glm::vec3& position, const Light& light, const Scene& scene);
}