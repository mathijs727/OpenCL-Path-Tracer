#include "material.h"
#include "scene.h"
#include "ray.h"
#include "light.h"
#include <glm\glm.hpp>
#include <algorithm>

#define RAYTRACER_EPSILON 0.0001f

namespace raytracer
{
	glm::vec3 whittedShading(
		const glm::vec3& rayDirection,
		const glm::vec3& intersection,
		const glm::vec3& normal,
		const Material& material,
		const Scene& scene)
	{

		// TODO: move this to the material class
		if (material.type == Material::Type::Diffuse)
		{
			glm::vec3 colour (0);
			for (const Light& light : scene.lights()) {
				glm::vec3 lightDir;
				if (get_light_vector(light, intersection, lightDir) && is_light_visible(intersection + normal * RAYTRACER_EPSILON, light, scene)) {
					float NdotL = std::max(0.0f, glm::dot(normal, lightDir));
					colour += material.colour * NdotL * light.colour;
				}
			}	
			return colour;
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