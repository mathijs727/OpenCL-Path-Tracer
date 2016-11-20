#include "material.h"
#include "scene.h"
#include "ray.h"
#include "light.h"
#include <glm\glm.hpp>
#include <algorithm>
#include <assert.h>

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
	float refractive_index_in,
	float refractive_index_out,
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	Ray& out_ray) {
	float cosine = glm::dot(rayDirection, -normal);
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

}