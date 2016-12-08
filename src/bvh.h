#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace raytracer {

class Scene;

struct AABB
{
	glm::vec3 centre;
	glm::vec3 extents;
	glm::vec3 min() { return centre - extents; }
	glm::vec3 max() { return centre + extents; }
};

struct FatBvhNode
{
	AABB bounds;
	glm::mat4 transform;
	glm::mat4 invTransform;
	union
	{
		int leftChildIndex;
		int thinBvhRoot;
	};
	int count;
};

struct ThinBvhNode
{
	AABB bounds;
	union
	{
		int leftChildIndex;
		int triangleFirstIndex;
	};
	int count;
};

class Bvh
{
	void build(Scene& scene);

	std::vector<FatBvhNode> _fatBuffer;
	std::vector<ThinBvhNode> _thinBuffer;
};
}

