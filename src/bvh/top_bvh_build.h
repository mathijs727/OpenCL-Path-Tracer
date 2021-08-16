#pragma once
#include "bvh_nodes.h"
#include <span>
#include <tuple>
#include <vector>

namespace raytracer {

struct SceneNode;

using BvhBuildReturnType = std::pair<uint32_t, std::vector<TopBVHNode>>;
BvhBuildReturnType buildTopBVH(const SceneNode& rootSceneNode, std::span<const uint32_t> meshBvhOffsets);

}
