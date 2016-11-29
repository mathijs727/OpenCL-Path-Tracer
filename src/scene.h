#pragma once
#include <vector>
#include "shapes.h"
#include "ray.h"
#include "material.h"
#include "light.h"
#include "mesh.h"

#define MAX_TRACE_DEPTH 6

namespace raytracer {

class Scene
{
public:
	Scene();
	// traces a ray through the objects in the scene, returns a colour
	template<typename TRay>
	glm::vec3 trace_ray(const TRay& ray, int current_depth = 0) const;
	bool check_ray(const Ray& ray) const;
	bool check_line(const Line& line) const;

	void add_primitive(const Sphere& primitive, const Material& material) {
		_spheres.push_back(primitive);
		_sphere_materials.push_back(material);
	}

	void add_primitive(const Plane& primitive, const Material& material) {
		_planes.push_back(primitive);
		_planes_materials.push_back(material);
	}

	void add_primitive(const Mesh& primitive, const Material& material) {
		_meshes.push_back(primitive);
		_meshes_materials.push_back(material);
	}
	
	void add_light(Light& light)
	{
		_lights.push_back(light);
	}

	const std::vector<Sphere>& GetSpheres() const { return _spheres; };
	const std::vector<Material>& GetSphereMaterials() const { return _sphere_materials; };

	const std::vector<Plane>& GetPlanes() const { return _planes; };
	const std::vector<Material>& GetPlaneMaterials() const { return _planes_materials; };

	const std::vector<Light>& GetLights() const { return _lights; };
public:
	struct LightIterableConst {
		const Scene& scene;

		LightIterableConst(const Scene& scene) : scene(scene) { }

		typedef std::vector<Light>::const_iterator const_iterator;

		const_iterator begin() { return scene._lights.cbegin(); }
		const_iterator end() { return scene._lights.cend(); }
	};

	LightIterableConst lights() const { return LightIterableConst(*this); }
public:
	float refractive_index = 1.000277f;
private:
	std::vector<Sphere> _spheres;
	std::vector<Material> _sphere_materials;

	std::vector<Plane> _planes;
	std::vector<Material> _planes_materials;

	std::vector<Mesh> _meshes;
	std::vector<Material> _meshes_materials;

	std::vector<Light> _lights;
};
}

#ifndef __SCENE_H_SHADING
#define __SCENE_H_SHADING
#include "shading.h"
#endif

template<typename TRay>
glm::vec3 raytracer::Scene::trace_ray(const TRay& ray, int current_depth) const {
	current_depth++;
	if (current_depth >= MAX_TRACE_DEPTH) return glm::vec3(0,0,0);

	enum class Type
	{
		Sphere,
		Plane,
		Mesh
	};

	// Find the closest intersection
	float min_intersect_time = std::numeric_limits<float>::max();
	int i_current_hit = -1;
	glm::vec3 normal_current_hit;
	Type type_current_hit;
	Triangle hit_triangle;
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
	for (unsigned int i = 0; i < _meshes.size(); ++i) {
		const Mesh& mesh = _meshes[i];
		for (auto& indices : mesh._triangleIndices) {
			float intersect_time;
			const Triangle triangle = mesh.getTriangle(indices);
			if (intersect(ray, triangle, intersect_time)) {
				min_intersect_time = intersect_time;
				type_current_hit = Type::Mesh;
				hit_triangle = triangle;
				i_current_hit = i;
			}
		}
	}

	if (i_current_hit >= 0) {
		// Calculate the normal of the hit surface and retrieve the material
		glm::vec3 direction = glm::normalize(ray.get_direction());
		glm::vec3 point = min_intersect_time * direction + ray.origin;
		glm::vec3 normal;
		Material material;
		if (type_current_hit == Type::Sphere)
		{
			normal = glm::normalize(point - _spheres[i_current_hit].centre);
			material = _sphere_materials[i_current_hit];
			return whittedShading(direction, point, normal, material, _spheres[i_current_hit], *this, current_depth);
		}
		else if (type_current_hit == Type::Plane)
		{
			normal = _planes[i_current_hit].normal;
			material = _planes_materials[i_current_hit];
			return whittedShading(direction, point, normal, material, _planes[i_current_hit], *this, current_depth);
		}
		else if (type_current_hit == Type::Mesh)
		{
			// TODO: check if the normal is always pointing to the correct side of the triangle
			const glm::vec3* points = hit_triangle.points;
			normal = glm::cross(
				points[1] - points[0],
				points[2] - points[0]);
			material = _meshes_materials[i_current_hit];
			return whittedShading(direction, point, normal, material, hit_triangle, *this, current_depth);
		}
	}
	
	return glm::vec3(0,0,0);
}