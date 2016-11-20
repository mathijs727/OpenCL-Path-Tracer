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
	const Scene& scene)
{
	glm::vec3 result_colour(0);
	for (const Light& light : scene.lights()) {
		glm::vec3 lightDir;
		if (get_light_vector(light, intersection, lightDir) && is_light_visible(intersection + normal * RAYTRACER_EPSILON, light, scene)) {
			float NdotL = std::max(0.0f, glm::dot(normal, lightDir));
			result_colour += NdotL * light.colour;
		}
	}
	return result_colour;
}

bool calc_refractive_ray(
	float refractive_index_out,
	float refractive_index_in,
	float cosine,
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	Ray& out_ray) {
	float refractive_ratio = refractive_index_in / refractive_index_out;
	float k = 1 - refractive_ratio*refractive_ratio * (1 - cosine*cosine); 
	if (k >= 0) {
		out_ray.direction = rayDirection * refractive_ratio + normal * (refractive_ratio * cosine - sqrt(k));
		out_ray.direction = glm::normalize(out_ray.direction);
		out_ray.origin = intersection - normal * RAYTRACER_EPSILON;
		return true;
	}
	else return false;
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
		return material.colour * diffuse_shade(rayDirection, intersection, normal, scene);
	}
	else if (material.type == Material::Type::Reflective)
	{
		Ray ray(intersection, glm::reflect(rayDirection, normal));
		return material.colour * scene.trace_ray(ray);
	}
	else if (material.type == Material::Type::Glossy) {
		Ray ray(intersection, glm::reflect(rayDirection, normal));
		glm::vec3 reflective_component = material.glossy.specularity * scene.trace_ray(ray);
		glm::vec3 diffuse_component = (1 - material.glossy.specularity) * diffuse_shade(rayDirection, intersection, normal, scene);
		return material.colour * (diffuse_component + reflective_component);
	}
	else if (material.type == Material::Type::Fresnel)
	{
		Ray reflective_ray(intersection, glm::reflect(rayDirection, normal));
		
		float cosine = glm::dot(-normal, rayDirection);
		Ray refractive_ray;
		if (calc_refractive_ray(scene.refractive_index, material.fresnel.refractive_index, cosine, rayDirection, intersection, normal, refractive_ray)) {
			float r0 = pow((-scene.refractive_index + material.fresnel.refractive_index) / (scene.refractive_index + material.fresnel.refractive_index), 2);
			float reflection_coeff = r0 + (1 - r0)*pow(1 - cosine, 5);
			glm::vec3 reflective_component = reflection_coeff * scene.trace_ray(reflective_ray);
			glm::vec3 refractive_component = (1 - reflection_coeff) * scene.trace_ray(refractive_ray);
			return material.colour * (reflective_component + reflective_component);
		}
		else {
			return material.colour * scene.trace_ray(reflective_ray);
		}
		
	}
	return glm::vec3(0);
}

}