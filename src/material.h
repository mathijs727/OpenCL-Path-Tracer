#pragma once
#include <glm.hpp>
#include "types.h"
#include "light.h"
#include <algorithm>

namespace raytracer {
class Scene;

struct Material
{
	enum class Type
	{
		Reflective,
		Diffuse,
		Glossy,
		Fresnel
	};

	Material() { }
	Material(const Material& m)
	{
		memcpy(this, &m, sizeof(Material));
	}
	Material& operator=(const Material& m)
	{
		memcpy(this, &m, sizeof(Material));
		return *this;
	}

	Type type;
	glm::vec3 colour;
	
	union
	{
		struct
		{
			float specularity;
		} glossy;
		struct
		{
			float refractive_index;
		} fresnel;
	};

	static Material Diffuse(const glm::vec3& colour) {
		Material result;
		result.type = Type::Diffuse;
		result.colour = colour;
		return result;
	}

	static Material Reflective(const glm::vec3& colour)
	{
		Material result;
		result.type = Type::Reflective;
		result.colour = colour;
		return result;
	}

	static Material Glossy(const glm::vec3& colour, float specularity) {
		Material result;
		result.type = Type::Glossy;
		result.colour = colour;
		result.glossy.specularity = std::min(std::max(specularity, 0.f),1.f);
		return result;
	}

	static Material Fresnel(const glm::vec3& colour, float refractive_index) {
		Material result;
		result.type = Type::Fresnel;
		result.colour = colour;
		result.fresnel.refractive_index = std::max(refractive_index, 1.f);
		return result;
	}
};

glm::vec3 diffuse_shade(
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	const Scene& scene);

bool calc_refractive_ray(
	float refractive_index_in,
	float refractive_index_out,
	float cosine,
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	Ray& out_ray);

template<typename TShape>
glm::vec3 whittedShading(
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	const Material& material,
	const TShape& shape,
	const Scene& scene) {

	// TODO: move this to the material class
	if (material.type == Material::Type::Diffuse) {
		return material.colour * diffuse_shade(rayDirection, intersection, normal, scene);
	}
	else if (material.type == Material::Type::Reflective) {
		Ray ray(intersection, glm::reflect(rayDirection, normal));
		return material.colour * scene.trace_ray(ray);
	}
	else if (material.type == Material::Type::Glossy) {
		Ray ray(intersection, glm::reflect(rayDirection, normal));
		glm::vec3 reflective_component = material.glossy.specularity * scene.trace_ray(ray);
		glm::vec3 diffuse_component = (1 - material.glossy.specularity) * diffuse_shade(rayDirection, intersection, normal, scene);
		return material.colour * (diffuse_component + reflective_component);
	}
	else if (material.type == Material::Type::Fresnel) {
		float n1 = scene.refractive_index;
		float n2 = material.fresnel.refractive_index;

		Ray reflective_ray(intersection, glm::reflect(rayDirection, normal));

		float cosine = glm::dot(-normal, rayDirection);
		Ray refractive_ray;
		if (calc_refractive_ray(n1, n2, cosine, rayDirection, intersection, normal, refractive_ray)) {
			float r0 = pow((n2 - n1) / (n2 + n1), 2);
			float reflection_coeff = r0 + (1 - r0)*pow(1 - cosine, 5);
			assert(0 < reflection_coeff && reflection_coeff < 1);
			

			glm::vec3 intersect_inside_normal;
			float intersect_inside_distance = intersect_inside(refractive_ray, shape, intersect_inside_normal);
			glm::vec3 inside_refraction_point = reflective_ray.origin + reflective_ray.direction * intersect_inside_distance;

			if (calc_refractive_ray(n2, n1, glm::dot(refractive_ray.direction, intersect_inside_normal),
				refractive_ray.direction, inside_refraction_point, intersect_inside_normal, refractive_ray))
			{
				glm::vec3 reflective_component = reflection_coeff * scene.trace_ray(reflective_ray);
				glm::vec3 refractive_component = (1 - reflection_coeff) * scene.trace_ray(refractive_ray);
				return material.colour * (refractive_component + reflective_component);
			}
		}
		return material.colour * scene.trace_ray(reflective_ray);
	}
	return glm::vec3(0);
}

}