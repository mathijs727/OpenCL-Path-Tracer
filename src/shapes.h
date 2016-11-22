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
	Plane(const glm::vec3& normal, const glm::vec3 pos)
	{
		this->normal = glm::normalize(normal);
		offset = glm::dot(pos, this->normal);
	}
	glm::vec3 normal;
	float offset;
};

struct Triangle
{
	glm::vec3 points[3];
};

bool intersect(const Ray& ray, const Sphere& sphere, float& time);
bool intersect(const Ray& ray, const Plane& plane, float& time);
bool intersect(const Ray& ray, const Triangle& triangle, float& time);

float intersect_inside(const Ray& ray, const Sphere& sphere, glm::vec3& out_normal);
float intersect_inside(const Ray& ray, const Plane& plane, glm::vec3& out_normal);
float intersect_inside(const Ray& ray, const Triangle& triangle, glm::vec3& out_normal);

bool intersect(const Line& line, const Sphere& sphere, float& time);
bool intersect(const Line& line, const Plane& plane, float& time);
bool intersect(const Line& line, const Triangle& triangle, float& time);

void calc_uv_coordinates(const Sphere& sphere, const glm::vec3& intersection, float& u, float& v);
void calc_uv_coordinates(const Plane& plane, const glm::vec3& intersection, float& u, float& v);
void calc_uv_coordinates(const Triangle& triangle, const glm::vec3& intersection, float& u, float& v);
}

