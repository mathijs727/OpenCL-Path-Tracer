#pragma once
#include "material.h"
#include "template/surface.h"
#include "scene.h"

inline uint32_t GetTextureValue(Tmpl8::Surface* tex, int x, int y)
{
	return *(tex->GetBuffer() + (y * tex->GetPitch()) + x);
}

namespace raytracer
{
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
		glm::vec3 diffuse_colour;
		/*if (material.diffuse.diffuse_texture != nullptr)
		{
			Tmpl8::Surface* texture = material.diffuse.diffuse_texture;

			float u, v;
			calc_uv_coordinates(shape, intersection, u, v);
			int x = static_cast<int>(u * texture->GetWidth());
			int y = static_cast<int>(v * texture->GetHeight());
			uint32_t pixel = GetTextureValue(texture, x, y);
			uint32_t r = (pixel & REDMASK) >> 16;
			uint32_t g = (pixel & GREENMASK) >> 8;
			uint32_t b = (pixel & BLUEMASK) >> 0;
			diffuse_colour = glm::vec3(r / 255.f, g / 255.f, b / 255.f);
		} else {
			diffuse_colour = material.colour;
		}*/
		return diffuse_colour * diffuse_shade(rayDirection, intersection, normal, scene);
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
		// the two refractive indexes
		float n1 = scene.refractive_index;
		float n2 = material.fresnel.refractive_index;
		float cosine = glm::dot(normal, -rayDirection);
		Ray refractive_ray;
		if (calc_refractive_ray(n1, n2, rayDirection, intersection, normal, refractive_ray)) {
			float r0 = pow((n1 - n2) / (n2 + n1), 2);
			float reflection_coeff = r0 + (1 - r0)*pow(1 - cosine, 5);
			assert(0 < reflection_coeff && reflection_coeff < 1);
			
			glm::vec3 intersect_inside_normal;
			float intersect_inside_distance = intersect_inside(refractive_ray, shape, intersect_inside_normal);
			glm::vec3 inside_refraction_point = refractive_ray.origin + refractive_ray.direction * intersect_inside_distance;

			Ray second_refractive_ray;
			bool good_ray = calc_refractive_ray(n1, n2, refractive_ray.direction, inside_refraction_point, intersect_inside_normal, second_refractive_ray);
			assert(good_ray);
			if (good_ray) {
				glm::vec3 reflective_component = reflection_coeff * scene.trace_ray(calc_reflective_ray(rayDirection, intersection, normal), current_depth);
				glm::vec3 refractive_component = (1 - reflection_coeff) * scene.trace_ray(second_refractive_ray, current_depth);
				return material.colour * (refractive_component + reflective_component);
			}
			else return glm::vec3(0,0,0); // should not happen
		}
		else {
			return material.colour * scene.trace_ray(calc_reflective_ray(rayDirection, intersection, normal), current_depth);
		}
	}
	return glm::vec3(0);
}
}