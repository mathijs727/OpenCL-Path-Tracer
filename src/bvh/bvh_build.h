#pragma once
#include "aabb.h"
#include "bvh_nodes.h"
#include "vertices.h"
#include <gsl/gsl>
#include <optional>
#include <tuple>
#include <vector>

namespace raytracer {

using BvhBuildReturnType = std::tuple<uint32_t, std::vector<TriangleSceneData>, std::vector<SubBvhNode>>;
BvhBuildReturnType buildBinnedBVH(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);
// Only considers splitting along the longest axis
BvhBuildReturnType buildBinnedFastBVH(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);

BvhBuildReturnType buildSpatialSplitBVH(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);

}
