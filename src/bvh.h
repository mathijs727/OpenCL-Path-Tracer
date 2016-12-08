#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "types.h"

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
		u32 leftChildIndex;
		u32 triangleFirstIndex;
	};
	u32 count;
};

class Bvh
{
public:
	Bvh(Scene& scene) : _scene(scene) {}
	void build();
private:
	void partition(ThinBvhNode& node, ThinBvhNode* bvhNodeBuffer, u32& nodeCount);
	void subdivide(ThinBvhNode& node, ThinBvhNode* buffer, u32& nodeCount);
	AABB create_bounds(u32 first_index, u32 count);
	Scene& _scene;
	std::vector<FatBvhNode> _fatBuffer;
	std::vector<ThinBvhNode> _thinBuffer;
};
}

