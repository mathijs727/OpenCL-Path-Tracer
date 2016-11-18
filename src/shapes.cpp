#include "shapes.h"
#include <assert.h>
#include <gtx/norm.hpp>

bool raytracer::intersect(const Ray& ray, const Sphere& sphere, IntersectionResult& intersection) {
	assert(glm::length2(ray.direction) - 1 < 1e-6); 
	glm::vec3 distance = sphere.centre - ray.origin;
	// calculates the projection of the distance on the ray
	float component_parallel = glm::dot(distance, ray.direction);
	glm::vec3 vec_parallel = component_parallel * ray.direction;
	// calculates the normal component to compare it with the radius
	glm::vec3 vec_normal = distance - component_parallel;
	float component_normal_squared = glm::length2(vec_normal);
	float radius_squared = sphere.radius * sphere.radius;
	if (component_normal_squared < radius_squared) {
		// the time is the "t" parameter in the P = O + rD equation of the ray
		// we use pythagoras's theorem to calc the missing cathetus
		intersection.time = component_parallel - sqrt(radius_squared - component_normal_squared);
		intersection.point = intersection.time * ray.direction + ray.origin;
		intersection.normal = glm::normalize(intersection.point - sphere.centre);
		return true;
	}
	return false;
}
