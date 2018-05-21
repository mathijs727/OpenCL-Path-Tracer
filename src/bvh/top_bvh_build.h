#pragma once
#include "bvh_nodes.h"
#include "types.h"
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <vector>

namespace raytracer {

struct SceneNode;

using BvhBuildReturnType = std::pair<uint32_t, std::vector<TopBVHNode>>;
BvhBuildReturnType buildTopBVH(const SceneNode& rootSceneNode, gsl::span<const uint32_t> meshBvhOffsets);

}
