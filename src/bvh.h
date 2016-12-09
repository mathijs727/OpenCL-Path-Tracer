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
		u32 firstTriangleIndex;
	};
	u32 triangleCount;

	ThinBvhNode()
	{
		leftChildIndex = 0;
		triangleCount = 0;
	}
};

class Bvh
{
public:
	Bvh(Scene& scene) : _scene(scene), _poolPtr(0) {}
	void build();

	const std::vector<ThinBvhNode>& GetThinNodes() { return _thinBuffer; };
private:
	u32 allocate();

	void subdivide(ThinBvhNode& node);
	void partition(ThinBvhNode& node, u32 leftIndex);
	AABB create_bounds(u32 first_index, u32 count);
private:
	Scene& _scene;

	std::vector<FatBvhNode> _fatBuffer;// TODO

	u32 _poolPtr;
	std::vector<ThinBvhNode> _thinBuffer;
};
}

