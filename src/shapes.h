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

struct Plane {
	glm::vec3 normal;
	float offset;
};

struct Triangle
{
	glm::vec3 points[3];
};

bool intersect(const Ray& ray, const Sphere& sphere, float& time);
bool intersect(const Ray& ray, const Plane& sphere, float& time);
bool intersect(const Ray& ray, const Triangle& sphere, float& time);

}

