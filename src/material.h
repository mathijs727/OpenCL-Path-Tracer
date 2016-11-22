#pragma once
#include <glm.hpp>
#include "types.h"
#include "light.h"
#include <algorithm>

#define RAYTRACER_EPSILON 0.0001f

namespace Tmpl8
{
	class Surface;
}

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
	~Material()
	{
		if (type == Type::Diffuse && diffuse.diffuse_texture != nullptr)
			delete diffuse.diffuse_texture;
	}

	Type type;
	glm::vec3 colour;
	
	union
	{
		struct
		{
			Tmpl8::Surface* diffuse_texture;
		} diffuse;
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
		result.diffuse.diffuse_texture = nullptr;
		return result;
	}

	static Material Diffuse(Tmpl8::Surface* diffuse) {
		Material result;
		result.type = Type::Diffuse;
		result.colour = glm::vec3(0);
		result.diffuse.diffuse_texture = diffuse;
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

}