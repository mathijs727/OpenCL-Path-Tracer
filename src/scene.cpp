#include "scene.h"
#include <algorithm>

using namespace raytracer;

raytracer::Scene::Scene() {
	_spheres.reserve(255);
	_sphere_materials.reserve(255);
}