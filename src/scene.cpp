#include "scene.h"
#include <algorithm>

raytracer::Scene::Scene() {
	_spheres.reserve(255);
	_sphere_materials.reserve(255);
}

glm::vec3 raytracer::Scene::trace_ray(const Ray& ray) const {
	enum class Type
	{
		Sphere,
		Plane,
		Triangle
	};

	// Find the closest intersection
	float min_intersect_time = std::numeric_limits<float>::max();
	int i_current_hit = -1;
	glm::vec3 normal_current_hit;
	Type type_current_hit;
	// Check sphere intersections
	for (unsigned int i = 0; i < _spheres.size(); ++i) {
		float intersect_time;
		if (intersect(ray, _spheres[i], intersect_time) && intersect_time < min_intersect_time) {
			min_intersect_time = intersect_time;
			type_current_hit = Type::Sphere;
			i_current_hit = i;
		}
	}
	// Check plane intersections
	for (unsigned int i = 0; i < _planes.size(); ++i) {
		float intersect_time;
		if (intersect(ray, _planes[i], intersect_time) && intersect_time < min_intersect_time) {
			min_intersect_time = intersect_time;
			type_current_hit = Type::Plane;
			i_current_hit = i;
		}
	}
	// Check triangle intersections
	for (unsigned int i = 0; i < _triangles.size(); ++i) {
		float intersect_time;
		if (intersect(ray, _triangles[i], intersect_time) && intersect_time < min_intersect_time) {
			min_intersect_time = intersect_time;
			type_current_hit = Type::Triangle;
			i_current_hit = i;
		}
	}



	if (i_current_hit >= 0) {
		// Calculate the normal of the hit surface and retrieve the material
		glm::vec3 point = min_intersect_time * ray.direction + ray.origin;
		glm::vec3 normal;
		Material material;
		if (type_current_hit == Type::Sphere)
		{
			normal = glm::normalize(point - _spheres[i_current_hit].centre);
			material = _sphere_materials[i_current_hit];
		}
		else if (type_current_hit == Type::Plane)
		{
			normal = _planes[i_current_hit].normal;
			material = _planes_materials[i_current_hit];
		}
		else if (type_current_hit == Type::Triangle)
		{
			// TODO: check if the normal is always pointing to the correct side of the triangle
			const glm::vec3* points = _triangles[i_current_hit].points;
			normal = glm::cross(
				points[1] - points[0],
				points[2] - points[0]);
			material = _triangles_materials[i_current_hit];
		}

		glm::vec3 colour = glm::vec3(0);
		for (auto& light : _lights)
		{
			glm::vec3 lightDir;
			if (get_light_vector(light, point, lightDir))
			{
				// TODO: move this to the material class
				if (material.type == Material::Type::Diffuse)
				{
					colour += material.colour * std::max(0.0f, glm::dot(normal, lightDir));
				}
				else if (material.type == Material::Type::Mirror)
				{
					Ray ray(ray.origin, glm::reflect(ray.direction, normal));
					colour += material.colour * trace_ray(ray);
				}
				else if (material.type == Material::Type::Diffuse)
				{
				}
			}
		}
		
		return colour;
	}
	else return glm::vec3(0,0,0);
}
