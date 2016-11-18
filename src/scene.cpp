#include "scene.h"

raytracer::Scene::Scene() {
	_spheres.reserve(255);
	_materials.reserve(255);
}

glm::vec3 raytracer::Scene::trace_ray(const Ray& ray) const {
	float min_intersect_time = std::numeric_limits<float>::max();
	int i_current_hit_sphere = -1;
	for (int i = 0; i < _spheres.size(); ++i) {
		float intersect_time;
		if (intersect(ray, _spheres[i], intersect_time) && intersect_time < min_intersect_time) {
			min_intersect_time = intersect_time;
			i = i_current_hit_sphere;
		}
	}
	if (i_current_hit_sphere >= 0) {
		glm::vec3 point = min_intersect_time * ray.direction + ray.origin;
		glm::vec3 normal = glm::normalize(point - _spheres[i_current_hit_sphere].centre);
		return _materials[i_current_hit_sphere].colour;
	}
	else return glm::vec3(0,0,0);
}
