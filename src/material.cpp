#include "material.h"
#include "scene.h"
#include "ray.h"
#include "light.h"
#include <glm.hpp>
#include <gtx/norm.hpp>
#include <algorithm>
#include <assert.h>
#include "template/surface.h"

#define PI 3.14159265359f

namespace raytracer
{

glm::vec3 diffuse_shade(
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	const Scene& scene)
{
	glm::vec3 result_light(0);
	for (const Light& light : scene.lights()) {
		glm::vec3 lightDir;
		if (get_light_vector(light, intersection, lightDir) && is_light_visible(intersection + normal * RAYTRACER_EPSILON, light, scene)) {
			float illuminance = 1.0f;
			if (light.type == Light::Type::Point)
			{
				float luminousIntensity = light.point.luminous_power / (4.0f * PI);
				float lightDistance2 = glm::length2(intersection - light.point.position);
				illuminance = (luminousIntensity / lightDistance2);// * (1 - (lightDistance2 / lightRadius2));
			}

			float NdotL = std::max(0.0f, glm::dot(normal, lightDir));
			result_light += NdotL * light.colour * illuminance;
		}
	}
	return result_light;
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