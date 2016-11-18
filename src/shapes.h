#pragma once

#include "ray.h"
#include <glm.hpp>

namespace raytracer {

struct Sphere
{
	glm::vec3 centre;
	float radius;
};

struct IntersectionResult
{
	glm::vec3 normal;
	glm::vec3 point;
	float time;
};

bool intersect(const Ray& ray, const Sphere& sphere, IntersectionResult& intersection);

struct Plane {
	glm::vec3 normal;
	float offset;
};

struct Triangle
{
	glm::vec3 points[3];
};

}

