#pragma once

#include "ray.h"
#include <glm.hpp>

namespace raytracer {

struct Sphere
{
	Sphere(const glm::vec3& centre, float radius) {
		this->centre = centre; this->radius = radius;
	}
	glm::vec3 centre;
	float radius;
};

bool intersect(const Ray& ray, const Sphere& sphere, float& time);

struct Plane {
	glm::vec3 normal;
	float offset;
};

struct Triangle
{
	glm::vec3 points[3];
};

}

