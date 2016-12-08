#pragma once
#include "types.h"
#include "ray.h"
#include <glm.hpp>

namespace raytracer {

struct Sphere
{
	Sphere() { }
	Sphere(const glm::vec3& centre, float radius) {
		this->centre = centre; this->radius = radius;
	}
	glm::vec3 centre; byte __padding1[4];// 16 bytes
	float radius; byte __padding2[12];// 16 bytes
};

struct Plane {
	Plane() { }
	Plane(const glm::vec3& normal, const glm::vec3 pos)
	{
		this->normal = glm::normalize(normal);
		offset = glm::dot(pos, this->normal);
		this->u_size = this->v_size = 0.0f;
	}
	Plane(const glm::vec3& normal, const glm::vec3 pos, float u_size, float v_size)
	{
		this->normal = glm::normalize(normal);
		offset = glm::dot(pos, this->normal);
		this->u_size = u_size;
		this->v_size = v_size;
	}
	glm::vec3 normal; byte __padding1[4];// 16 bytes

	// 16 bytes
	float offset;
	float u_size, v_size;// World coordinate size of texture mapped to the plane
	byte __padding2[4];
};

struct Triangle
{
	glm::vec3 points[3];
};

}

