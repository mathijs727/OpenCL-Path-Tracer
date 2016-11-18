#pragma once

#include <glm.hpp>

namespace raytracer {

struct Sphere
{
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

}

