#pragma once
#include <vector>
#include "shapes.h"
#include "ray.h"

namespace raytracer {

class Scene
{
public:
	// traces a ray through the objects in the scene, returns a colour
	glm::vec3 trace_ray(const Ray& ray);
private:
	std::vector<Sphere> _spheres;
};

}