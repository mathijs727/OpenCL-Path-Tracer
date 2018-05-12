#pragma once
#include "aabb.h"
#include "bvh_nodes.h"
#include "types.h"
#include "vertices.h"
#include <gsl/gsl>
#include <tuple>
#include <vector>

namespace raytracer {

std::tuple<uint32_t, std::vector<TriangleSceneData>, std::vector<SubBvhNode>> buildBinnedBVH(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);
}
