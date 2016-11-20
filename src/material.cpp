#include "material.h"
#include "scene.h"
#include "ray.h"
#include "light.h"
#include <glm\glm.hpp>
#include <algorithm>

#define RAYTRACER_EPSILON 0.0001f

namespace raytracer
{

glm::vec3 diffuse_shade(
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	const glm::vec3& colour,
	const Scene& scene)
{
	glm::vec3 result_colour(0);
	for (const Light& light : scene.lights()) {
		glm::vec3 lightDir;
		if (get_light_vector(light, intersection, lightDir) && is_light_visible(intersection + normal * RAYTRACER_EPSILON, light, scene)) {
			float NdotL = std::max(0.0f, glm::dot(normal, lightDir));
			result_colour += colour * NdotL * light.colour;
		}
	}
	return result_colour;
}

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
		return diffuse_shade(rayDirection, intersection, normal, material.colour, scene);
	}
	else if (material.type == Material::Type::Reflective)
	{
		Ray ray(intersection, glm::reflect(rayDirection, normal));
		return material.colour * scene.trace_ray(ray);
	}
	else if (material.type == Material::Type::Glossy) {
		Ray ray(intersection, glm::reflect(rayDirection, normal));
		glm::vec3 diffuse_component = (1 - material.glossy.specularity) * diffuse_shade(rayDirection, intersection, normal, material.colour, scene);
		glm::vec3 specular_component = material.glossy.specularity * material.colour * scene.trace_ray(ray);
		return diffuse_component + specular_component;
	}
	else if (material.type == Material::Type::Fresnel)
	{
	}
	return glm::vec3(0);
}

}