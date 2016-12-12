#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "types.h"
#include "template\includes.h"

namespace raytracer {

class Scene;
struct SceneNode;

struct AABB
{
	union
	{
		glm::vec3 centre;
		cl_float3 __centre_cl;
	};
	union
	{
		glm::vec3 extents;
		cl_float3 __extents_cl;
	};
	//glm::vec3 centre;
	//glm::vec3 extents;
	glm::vec3 min() { return centre - extents; }
	glm::vec3 max() { return centre + extents; }

	AABB() { };
	AABB(const AABB& other)
	{
		memcpy(this, &other, sizeof(AABB));
	}
	AABB& operator=(const AABB& other)
	{
		memcpy(this, &other, sizeof(AABB));
		return *this;
	}
};

struct FatBvhNode
{
	AABB bounds;
	glm::mat4 transform;
	glm::mat4 invTransform;
	union
	{
		struct
		{
			u32 leftChildIndex;
			u32 rightChildIndex;
		};
		u32 thinBvh;
	};
	u32 isLeaf;
	u32 __padding;// Make the FatBvhNode 16 byte aligned
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
	u32 __padding_cl[2];

	ThinBvhNode()
	{
		leftChildIndex = 0;
		triangleCount = 0;
	}
};

class Bvh
{
public:
	Bvh(Scene& scene) : _scene(scene), _thinPoolPtr(0) {}

	void buildThinBvhs();
	void updateTopLevelBvh();

	const std::vector<FatBvhNode>& GetFatNodes() { return _fatBuffer; };
	const std::vector<ThinBvhNode>& GetThinNodes() { return _thinBuffer; };
private:
	u32 allocateThinNode();

	u32 findBestMatch(const std::vector<u32>& list, u32 nodeId);
	FatBvhNode createFatNode(const SceneNode* node, const glm::mat4 transform);
	FatBvhNode mergeFatNodes(u32 nodeId1, u32 nodeId2);
	AABB calcCombinedBounds(const AABB& bounds1, const AABB& bounds2);
	AABB calcTransformedAABB(const AABB& bounds, glm::mat4 transform);

	void subdivide(ThinBvhNode& node);
	void partition(ThinBvhNode& node, u32 leftIndex);
	AABB create_bounds(u32 first_index, u32 count);
private:
	Scene& _scene;

	u32 _fatPoolPtr;
	std::vector<FatBvhNode> _fatBuffer;

	u32 _thinPoolPtr;
	std::vector<ThinBvhNode> _thinBuffer;
};
}

