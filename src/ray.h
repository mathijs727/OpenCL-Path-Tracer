#pragma once

#include <glm.hpp>

struct Ray
{
	Ray(const glm::vec3& origin, const glm::vec3& direction) {
		this->origin = origin;
		this->direction = direction;
	}
	glm::vec3 origin;
	glm::vec3 direction;
};

