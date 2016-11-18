#include "scene.h"
#include <algorithm>

raytracer::Scene::Scene() {
	_spheres.reserve(255);
	_sphere_materials.reserve(255);
}

glm::vec3 raytracer::Scene::trace_ray(const Ray& ray) const {
	// Find the closet intersection
	float min_intersect_time = std::numeric_limits<float>::max();
	int i_current_hit_sphere = -1;
	for (unsigned int i = 0; i < _spheres.size(); ++i) {
		float intersect_time;
		if (intersect(ray, _spheres[i], intersect_time) && intersect_time < min_intersect_time) {
			min_intersect_time = intersect_time;
			i_current_hit_sphere = i;
		}
	}

	if (i_current_hit_sphere >= 0) {
		glm::vec3 point = min_intersect_time * ray.direction + ray.origin;
		glm::vec3 normal = glm::normalize(point - _spheres[i_current_hit_sphere].centre);

		const Material& mat = _sphere_materials[i_current_hit_sphere];
		glm::vec3 colour = glm::vec3(0);
		for (auto& light : _lights)
		{
			glm::vec3 lightDir;
			if (get_light_vector(light, point, lightDir))
			{
				if (mat.type == Material::Type::Diffuse)
				{
					colour += std::max(0.0f, glm::dot(normal, lightDir)) * mat.colour;
				}
				else if (mat.type == Material::Type::Mirror)
				{
					Ray ray(ray.origin, glm::reflect(ray.direction, normal));
					colour += mat.colour * trace_ray(ray);
				}
			}
			else if (mat.type == Material::Type::Diffuse)
			{

			}
		}
		
		return colour;
	}
	else return glm::vec3(0,0,0);
}
