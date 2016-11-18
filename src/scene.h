#pragma once
#include <vector>
#include "shapes.h"
#include "ray.h"

namespace raytracer {

class Scene
{
public:
	glm::vec3 trace_ray();
private:
	std::vector<Sphere> _spheres;
};

}