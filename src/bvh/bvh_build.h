#pragma once
#include "bvh_nodes.h"
#include "vertices.h"
#include <span>
#include <tuple>
#include <vector>

namespace raytracer {

using BvhBuildReturnType = std::tuple<uint32_t, std::vector<TriangleSceneData>, std::vector<SubBVHNode>>;
BvhBuildReturnType buildBinnedBVH(std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles);
// Only considers splitting along the longest axis
BvhBuildReturnType buildBinnedFastBVH(std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles);

BvhBuildReturnType buildSpatialSplitBVH(std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles);

}
