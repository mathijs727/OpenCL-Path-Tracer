#pragma once

#include <glm.hpp>

struct Ray
{
	Ray() = default;
	Ray(const glm::vec3& origin, const glm::vec3& direction) {
		this->origin = origin;
		this->direction = glm::normalize(direction);
	}
	glm::vec3 origin;
	glm::vec3 direction;

	glm::vec3 get_direction() const { return direction; }
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
	
	glm::vec3 get_direction() const { return glm::normalize(dest - origin); }
};