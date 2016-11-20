#include "material.h"
#include "scene.h"
#include "ray.h"
#include "light.h"
#include <glm\glm.hpp>
#include <algorithm>

namespace raytracer
{
	glm::vec3 whittedShading(
		const Light& light,
		const glm::vec3& rayDirection,
		const glm::vec3& intersection,
		const glm::vec3& normal,
		const glm::vec3& lightDir,
		const Material& material,
		const Scene& scene)
	{
		// TODO: move this to the material class
		if (material.type == Material::Type::Diffuse)
		{
			if (is_light_visible(intersection, light, scene))
			{
				float NdotL = std::max(0.0f, glm::dot(normal, lightDir));
				return material.colour * NdotL * light.colour;
			}
		}
		else if (material.type == Material::Type::Mirror)
		{
			Ray ray(intersection, glm::reflect(rayDirection, normal));
			return material.colour * scene.trace_ray(ray);
		}
		else if (material.type == Material::Type::Fresnel)
		{
		}
		return glm::vec3(0);
	}
}