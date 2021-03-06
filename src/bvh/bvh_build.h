#pragma once
#include "bvh_nodes.h"
#include "vertices.h"
#include <gsl/gsl>
#include <tuple>
#include <vector>

namespace raytracer {

using BvhBuildReturnType = std::tuple<uint32_t, std::vector<TriangleSceneData>, std::vector<SubBVHNode>>;
BvhBuildReturnType buildBinnedBVH(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);
// Only considers splitting along the longest axis
BvhBuildReturnType buildBinnedFastBVH(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);

BvhBuildReturnType buildSpatialSplitBVH(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);

}
