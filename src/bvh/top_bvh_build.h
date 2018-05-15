#pragma once
#include "bvh_nodes.h"
#include "types.h"
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <vector>

namespace raytracer {

struct SceneNode;

using BvhBuildReturnType = std::pair<uint32_t, std::vector<TopBvhNode>>;
BvhBuildReturnType buildTopBVH(const SceneNode& rootSceneNode, gsl::span<const uint32_t> meshBvhOffsets);

/*class TopLevelBvhBuilder {
public:
    TopLevelBvhBuilder(Scene& scene)
        : _scene(scene){};

    uint32_t build(std::vector<SubBvhNode>& subBvhNodes,
        std::vector<TopBvhNode>& outTopNodes);

private:
    uint32_t findBestMatch(const std::vector<uint32_t>& list, uint32_t nodeId);
    TopBvhNode createNode(const SceneNode* node, const glm::mat4 transform);
    TopBvhNode mergeNodes(uint32_t nodeId1, uint32_t nodeId2);
    AABB calcCombinedBounds(const AABB& bounds1, const AABB& bounds2);
    AABB calcTransformedAABB(const AABB& bounds, glm::mat4 transform);

private:
    Scene& _scene;

    // Used during construction because passing it through recursion is ugly
    std::vector<TopBvhNode>* _top_bvh_nodes;
    std::vector<SubBvhNode>* _sub_bvh_nodes;
};*/
}
