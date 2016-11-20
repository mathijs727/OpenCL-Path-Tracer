#pragma once
#include <vector>
#include "shapes.h"
#include "ray.h"
#include "material.h"
#include "light.h"

namespace raytracer {

class Scene
{
public:
	Scene();
	// traces a ray through the objects in the scene, returns a colour
	glm::vec3 trace_ray(const Ray& ray) const;
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

	void add_primitive(const Triangle& primitive, const Material& material) {
		_triangles.push_back(primitive);
		_triangles_materials.push_back(material);
	}
	
	void add_light(Light& light)
	{
		_lights.push_back(light);
	}
private:
	std::vector<Sphere> _spheres;
	std::vector<Material> _sphere_materials;

	std::vector<Plane> _planes;
	std::vector<Material> _planes_materials;

	std::vector<Triangle> _triangles;
	std::vector<Material> _triangles_materials;

	std::vector<Light> _lights;
};

}