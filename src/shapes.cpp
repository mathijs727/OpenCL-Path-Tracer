#include "shapes.h"
#include <assert.h>
#include <gtx/norm.hpp>

bool raytracer::intersect(const Ray& ray, const Sphere& sphere, float& time) {
	assert(glm::length2(ray.direction) - 1 < 1e-6); 
	glm::vec3 distance = sphere.centre - ray.origin;
	// calculates the projection of the distance on the ray
	float component_parallel = glm::dot(distance, ray.direction);
	glm::vec3 vec_parallel = component_parallel * ray.direction;
	// calculates the normal component to compare it with the radius
	glm::vec3 vec_normal = distance - vec_parallel;
	float component_normal_squared = glm::length2(vec_normal);
	float radius_squared = sphere.radius * sphere.radius;
	if (component_normal_squared < radius_squared) {
		// the time is the "t" parameter in the P = O + rD equation of the ray
		// we use pythagoras's theorem to calc the missing cathetus
		float t = component_parallel - sqrt(radius_squared - component_normal_squared);
		if (t > 0.0f)
		{
			time = t;
			return true;
		}
		else {
			return false;
		}
	}
	return false;
}

float raytracer::intersect_inside(const Ray& ray, const Sphere& sphere, glm::vec3& out_normal) {
	assert(glm::length2(ray.origin - sphere.centre) < pow(sphere.radius,2));
	glm::vec3 distance = sphere.centre - ray.origin;
	// calculates the projection of the distance on the ray
	float component_parallel = glm::dot(distance, ray.direction);
	glm::vec3 vec_parallel = component_parallel * ray.direction;
	// calculates the normal component to compare it with the radius
	glm::vec3 vec_normal = distance - vec_parallel;
	float component_normal_squared = glm::length2(vec_normal);
	float radius_squared = sphere.radius * sphere.radius;
	
	// the time is the "t" parameter in the P = O + rD equation of the ray
	// we use pythagoras's theorem to calc the missing cathetus
	float t = component_parallel + sqrt(radius_squared - component_normal_squared);
	assert(t > 0.0f);
	out_normal = glm::normalize(sphere.centre - (ray.origin + ray.direction * t));
	return t;
}

float raytracer::intersect_inside(const Ray& ray, const Triangle& sphere, glm::vec3& out_normal) {
	return 0;
}

float raytracer::intersect_inside(const Ray& ray, const Plane& plane, glm::vec3& out_normal) {
	out_normal = plane.normal;
	return 0;
}

bool raytracer::intersect(const Ray& ray, const Plane& plane, float& time)
{
	// http://stackoverflow.com/questions/23975555/how-to-do-ray-plane-intersection
	float denom = glm::dot(plane.normal, ray.direction);
    if (std::fabs(denom) > 1e-6)// Check that ray not parallel to plane
	{
		// A known point on the plane
		glm::vec3 center = plane.offset * plane.normal;
		float t = glm::dot(center - ray.origin, plane.normal) / denom;
		if (t > 0.0f)
		{
			time = t;
			return true;
		}
	}
	return false;
}

bool raytracer::intersect(const Ray& ray, const Triangle& triangle, float& time)
{
	return false;
}



bool raytracer::intersect(const Line& line, const Sphere& sphere, float& time)
{
	float t;
	glm::vec3 dir = line.dest - line.origin;
	float maxT2 = glm::length2(dir);
	if (intersect(Ray(line.origin, glm::normalize(dir)), sphere, t) && (t*t) < maxT2)
	{
		time = t;
		return true;
	}
	return false;
}

bool raytracer::intersect(const Line& line, const Plane& plane, float & time)
{
	float t;
	glm::vec3 dir = line.dest - line.origin;
	float maxT2 = glm::length2(dir);
	if (intersect(Ray(line.origin, glm::normalize(dir)), plane, t) && (t*t) < maxT2)
	{
		time = t;
		return true;
	}
	return false;
}

bool raytracer::intersect(const Line& line, const Triangle& triangle, float & time)
{
	float t;
	glm::vec3 dir = line.dest - line.origin;
	float maxT2 = glm::length2(dir);
	if (intersect(Ray(line.origin, glm::normalize(dir)), triangle, t) && (t*t) < maxT2)
	{
		time = t;
		return true;
	}
	return false;
}
