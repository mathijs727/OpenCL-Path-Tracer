#pragma once
#include "bvh_nodes.h"
#include <gsl/gsl>
#include <tuple>
#include <vector>

namespace raytracer {

struct SceneNode;

using BvhBuildReturnType = std::pair<uint32_t, std::vector<TopBVHNode>>;
BvhBuildReturnType buildTopBVH(const SceneNode& rootSceneNode, gsl::span<const uint32_t> meshBvhOffsets);

}
