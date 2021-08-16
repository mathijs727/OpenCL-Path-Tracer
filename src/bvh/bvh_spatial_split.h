#pragma once
#include "aabb.h"
#include "bvh_nodes.h"
#include <span>
#include <optional>
#include <tuple>

// Spatial split BVH
// http://www.nvidia.com/docs/IO/77714/sbvh.pdf

namespace raytracer {

struct SpatialSplit {
    int axis;
    float position;
    size_t leftCount, rightCount;
    AABB leftBounds;
    AABB rightBounds;
    float partialSAH = std::numeric_limits<float>::max();
};

std::optional<SpatialSplit> findSpatialSplitBinned(
    const AABB& nodeBounds,
    std::span<const PrimitiveData> primitives,
    const OriginalPrimitives& orignalPrimitives,
    std::span<const int> axisToConsider);
std::pair<AABB, AABB> performSpatialSplit(std::span<const PrimitiveData> primitives, const OriginalPrimitives& originalPrimitives, const SpatialSplit& split, PrimInsertIter left, PrimInsertIter right);

}
