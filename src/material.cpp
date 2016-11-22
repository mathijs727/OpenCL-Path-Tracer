#include "material.h"
#include "scene.h"
#include "ray.h"
#include "light.h"
#include <glm.hpp>
#include <algorithm>
#include <assert.h>
#include "template/surface.h"

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
	float n1,
	float n2,
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	Ray& out_ray) {
	float cosine = glm::dot(-rayDirection, normal);
	float refractive_ratio = n1 / n2;
	float k = 1 - ( pow(refractive_ratio,2) * (1 - pow(cosine,2)) ); 
	if (k >= 0) {
		out_ray.direction = glm::normalize(rayDirection * refractive_ratio + normal * (refractive_ratio * cosine - sqrtf(k)));
		out_ray.origin = intersection - normal * RAYTRACER_EPSILON;
		return true;
	}
	else return false;
}

}