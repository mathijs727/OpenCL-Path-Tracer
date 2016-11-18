#pragma once
#include <vector>
#include "shapes.h"
#include "ray.h"
#include "material.h"

namespace raytracer {

class Scene
{
public:
	Scene();
	// traces a ray through the objects in the scene, returns a colour
	glm::vec3 trace_ray(const Ray& ray) const;
private:
	std::vector<Sphere> _spheres;
	std::vector<Material> _materials;
};

}