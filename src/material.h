#pragma once
#include <glm.hpp>
#include "types.h"
#include "light.h"
#include <algorithm>

#define RAYTRACER_EPSILON 0.0001f

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
	float n1,
	float n2,
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	Ray& out_ray);

inline Ray calc_reflective_ray(
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal) {
	return Ray(intersection + normal * RAYTRACER_EPSILON, glm::reflect(rayDirection, normal));
}

template<typename TShape>
glm::vec3 whittedShading(
	const glm::vec3& rayDirection,
	const glm::vec3& intersection,
	const glm::vec3& normal,
	const Material& material,
	const TShape& shape,
	const Scene& scene,
	int current_depth) {

	// TODO: move this to the material class
	if (material.type == Material::Type::Diffuse) {
		return material.colour * diffuse_shade(rayDirection, intersection, normal, scene);
	}
	else if (material.type == Material::Type::Reflective) {
		return material.colour * scene.trace_ray(calc_reflective_ray(rayDirection, intersection, normal), current_depth);
	}
	else if (material.type == Material::Type::Glossy) {
		glm::vec3 reflective_component = material.glossy.specularity * scene.trace_ray(calc_reflective_ray(rayDirection, intersection, normal), current_depth);
		glm::vec3 diffuse_component = (1 - material.glossy.specularity) * diffuse_shade(rayDirection, intersection, normal, scene);
		return material.colour * (diffuse_component + reflective_component);
	}
	else if (material.type == Material::Type::Fresnel) {
		float n1 = scene.refractive_index;
		float n2 = material.fresnel.refractive_index;
		float cosine = glm::dot(-normal, rayDirection);
		Ray refractive_ray;
		if (calc_refractive_ray(n1, n2, rayDirection, intersection, normal, refractive_ray)) {
			float r0 = pow((n1 - n2) / (n2 + n1), 2);
			float reflection_coeff = r0 + (1 - r0)*pow(1 - cosine, 5);
			assert(0 < reflection_coeff && reflection_coeff < 1);
			
			glm::vec3 intersect_inside_normal;
			float intersect_inside_distance = intersect_inside(refractive_ray, shape, intersect_inside_normal);
			glm::vec3 inside_refraction_point = refractive_ray.origin + refractive_ray.direction * intersect_inside_distance;

			Ray second_refractive_ray;
			if (calc_refractive_ray(n2, n1,
				refractive_ray.direction, inside_refraction_point, intersect_inside_normal, second_refractive_ray))
			{
				glm::vec3 reflective_component = reflection_coeff * scene.trace_ray(calc_reflective_ray(rayDirection, intersection, normal), current_depth);
				glm::vec3 refractive_component = (1 - reflection_coeff) * scene.trace_ray(second_refractive_ray, current_depth);
				return material.colour * (refractive_component + reflective_component);
			}
			else return glm::vec3(1,1,1);
		}
		else {
			material.colour * scene.trace_ray(calc_reflective_ray(rayDirection, intersection, normal), current_depth);
		}
	}
	return glm::vec3(0);
}

}