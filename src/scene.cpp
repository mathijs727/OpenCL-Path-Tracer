#include "scene.h"
#include <algorithm>

raytracer::Scene::Scene() {
	_spheres.reserve(255);
	_sphere_materials.reserve(255);
}

bool raytracer::Scene::check_ray(const Ray & ray) const
{
	// Check sphere intersections
	for (unsigned int i = 0; i < _spheres.size(); ++i) {
		float intersect_time;
		if (intersect(ray, _spheres[i], intersect_time)) {
			return true;
		}
	}
	// Check plane intersections
	for (unsigned int i = 0; i < _planes.size(); ++i) {
		float intersect_time;
		if (intersect(ray, _planes[i], intersect_time)) {
			return true;
		}
	}
	// Check triangle intersections
	for (unsigned int i = 0; i < _triangles.size(); ++i) {
		float intersect_time;
		if (intersect(ray, _triangles[i], intersect_time)) {
			return true;
		}
	}

	return false;
}

bool raytracer::Scene::check_line(const Line & line) const
{
	// Check sphere intersections
	for (unsigned int i = 0; i < _spheres.size(); ++i) {
		float intersect_time;
		if (intersect(line, _spheres[i], intersect_time)) {
			return true;
		}
	}
	// Check plane intersections
	for (unsigned int i = 0; i < _planes.size(); ++i) {
		float intersect_time;
		if (intersect(line, _planes[i], intersect_time)) {
			return true;
		}
	}
	// Check triangle intersections
	for (unsigned int i = 0; i < _triangles.size(); ++i) {
		float intersect_time;
		if (intersect(line, _triangles[i], intersect_time)) {
			return true;
		}
	}

	return false;
}
