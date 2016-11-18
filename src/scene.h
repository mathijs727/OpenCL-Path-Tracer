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
	template<typename TShape>
	void add_primitive(TShape&& primitive, const Material& material) {
		_spheres.push_back(primitive);
		_sphere_materials.push_back(material);
	}
	
	void add_light(Light& light)
	{
		_lights.push_back(light);
	}
private:
	std::vector<Sphere> _spheres;
	std::vector<Material> _sphere_materials;

	std::vector<Light> _lights;
};

}