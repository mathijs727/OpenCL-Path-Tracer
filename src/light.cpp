#include "light.h"
#include "scene.h"

bool raytracer::get_light_vector(const Light& light, const glm::vec3 position, glm::vec3& lightDirection)
{
	if (light.type == Light::Type::Point)
	{
		lightDirection = glm::normalize(light.point.position - position);
		return true;
	}
	else if (light.type == Light::Type::Directional)
	{
		lightDirection = -light.directional.direction;
		return true;
	}

	return false;
}

bool raytracer::is_light_visible(const glm::vec3& position, const Light& light, const Scene& scene)
{
	if (light.type == Light::Type::Directional)
	{
		Ray ray(position, -light.directional.direction);
		return !scene.check_ray(ray);
	}
	else if (light.type == Light::Type::Point)
	{
		if (position.y > 0.6)
			_asm nop;
		Line line(position, light.point.position);
		return !scene.check_line(line);
	}
	return false;
}
