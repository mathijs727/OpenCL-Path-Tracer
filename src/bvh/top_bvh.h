#pragma once
#include "bvh_nodes.h"
#include "types.h"
#include <glm/glm.hpp>
#include <vector>

namespace raytracer {

class Scene;
struct SceneNode;

class TopLevelBvhBuilder {
public:
    TopLevelBvhBuilder(Scene& scene)
        : _scene(scene){};

    uint32_t build(std::vector<SubBVHNode>& subBvhNodes,
        std::vector<TopBVHNode>& outTopNodes);

private:
    uint32_t findBestMatch(const std::vector<uint32_t>& list, uint32_t nodeId);
    TopBVHNode createNode(const SceneNode* node, const glm::mat4 transform);
    TopBVHNode mergeNodes(uint32_t nodeId1, uint32_t nodeId2);
    AABB calcCombinedBounds(const AABB& bounds1, const AABB& bounds2);
    AABB calcTransformedAABB(const AABB& bounds, glm::mat4 transform);

private:
    Scene& _scene;

    // Used during construction because passing it through recursion is ugly
    std::vector<TopBVHNode>* _top_bvh_nodes;
    std::vector<SubBVHNode>* _sub_bvh_nodes;
};
}
