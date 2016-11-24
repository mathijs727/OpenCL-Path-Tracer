#include "shapes.h"
#include <assert.h>
#include <math.h>
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

// https://en.wikipedia.org/wiki/Möller–Trumbore_intersection_algorithm
bool raytracer::intersect(const Ray& ray, const Triangle& triangle, float& time)
{
	const float EPSILON = 0.000001f;

	glm::vec3 e1, e2;  //Edge1, Edge2
	glm::vec3 P, Q, T;
	float det, inv_det, u, v;
	float t;

	//Find vectors for two edges sharing V1
	e1 = triangle.points[1] - triangle.points[0];
	e2 = triangle.points[2] - triangle.points[0];

	//Begin calculating determinant - also used to calculate u parameter
	P = glm::cross(ray.direction, e2);

	//if determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
	det = glm::dot(e1, P);

	//NOT CULLING
	if(det > -EPSILON && det < EPSILON) return false;
	inv_det = 1.f / det;

	//calculate distance from V1 to ray origin
	T = ray.origin - triangle.points[0];

	//Calculate u parameter and test bound
	u = glm::dot(T, P) * inv_det;
	//The intersection lies outside of the triangle
	if(u < 0.f || u > 1.f) return false;

	//Prepare to test v parameter
	Q = glm::cross(T, e1);

	//Calculate V parameter and test bound
	v = glm::dot(ray.direction, Q) * inv_det;
	//The intersection lies outside of the triangle
	if(v < 0.f || u + v  > 1.f) return false;

	t = glm::dot(e2, Q) * inv_det;

	if(t > EPSILON) { //ray intersection
		time = t;
		return true;
	}

	// No hit, no win
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

void raytracer::calc_uv_coordinates(const Sphere& sphere, const glm::vec3& intersection, float& u, float& v)
{
	const float pi = 3.14159265f;

	glm::vec3 d = sphere.centre - intersection;
	u = 0.5f + atan2(d.z, d.x) / 2 / pi;
	v = 0.5f - atan(d.y) / pi;
}

void raytracer::calc_uv_coordinates(const Plane& plane, const glm::vec3& intersection, float& u, float& v)
{
	// TODO: this doesnt work if plane normal = (1, 0, 0)
	glm::vec3 tmp = plane.normal;
	tmp.x += 1.0f;
	tmp = glm::normalize(tmp);

	glm::vec3 plane_x = glm::cross(tmp, plane.normal);
	glm::vec3 plane_y = glm::cross(plane_x, tmp);

	u = fmod(fabs(glm::dot(plane_x, intersection)), 1.0f);
	v = fmod(fabs(glm::dot(plane_y, intersection)), 1.0f);
}


void raytracer::calc_uv_coordinates(const Triangle& triangle, const glm::vec3& intersection, float& u, float& v)
{
	u = 0.0f;
	v = 0.0f;
}




