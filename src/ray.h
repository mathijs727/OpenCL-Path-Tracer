#pragma once

#include <glm.hpp>

struct Ray
{
	Ray() =delete;
	Ray(const glm::vec3& origin, const glm::vec3& direction) {
		this->origin = origin;
		this->direction = direction;
	}
	glm::vec3 origin;
	glm::vec3 direction;
};

struct Line
{
	Line() = delete;
	Line(const glm::vec3& origin, const glm::vec3& dest) {
		this->origin = origin;
		this->dest = dest;
	}
	glm::vec3 origin;
	glm::vec3 dest;
};