#pragma once
#include <glm.hpp>
#include <vector>
#include "types.h"
#include "template/includes.h"
#include "bvh_nodes.h"
#include "linear_allocator.h"

namespace raytracer {

class Scene;
struct SceneNode;

class TopLevelBvhBuilder
{
public:
	TopLevelBvhBuilder(Scene& scene) : _scene(scene) { };

	void build();
private:
	u32 findBestMatch(const std::vector<u32>& list, u32 nodeId);
	TopBvhNode createNode(const SceneNode* node, const glm::mat4 transform);
	TopBvhNode mergeNodes(u32 nodeId1, u32 nodeId2);
	AABB calcCombinedBounds(const AABB& bounds1, const AABB& bounds2);
	AABB calcTransformedAABB(const AABB& bounds, glm::mat4 transform);
private:
	Scene& _scene;
};
}

