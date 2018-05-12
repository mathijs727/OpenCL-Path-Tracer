#pragma once
#include "aabb.h"
#include "bvh_nodes.h"
#include "types.h"
#include "vertices.h"
#include <cstdint>
#include <gsl/gsl>
#include <limits>
#include <optional>
#include <tuple>
#include <vector>

namespace raytracer {

//std::tuple<uint32_t, std::vector<TriangleSceneData>, std::vector<SubBvhNode>> buildBinnedBVH(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);

struct ObjectSplit {
    int axis;
    float position;

    AABB leftBounds;
    AABB rightBounds;
    float sah = std::numeric_limits<float>::max();
};

using PrimInsertIter = std::insert_iterator<std::vector<PrimitiveData>>;
std::optional<ObjectSplit> findObjectSplitBinned(
    const SubBvhNode& node,
    gsl::span<const PrimitiveData> primitives,
    gsl::span<const int> axisToConsider = std::array{ 0, 1, 2 });
void performObjectSplit(gsl::span<const PrimitiveData> primitives, const ObjectSplit& split, PrimInsertIter left, PrimInsertIter right);
size_t performObjectSplitInPlace(gsl::span<PrimitiveData> primitives, const ObjectSplit& split);

}
