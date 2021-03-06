#pragma once
#include "aabb.h"
#include "bvh_nodes.h"
#include <gsl/gsl>
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
    gsl::span<const PrimitiveData> primitives,
    const OriginalPrimitives& orignalPrimitives,
    gsl::span<const int> axisToConsider = std::array{ 0, 1, 2 });
std::pair<AABB, AABB> performSpatialSplit(gsl::span<const PrimitiveData> primitives, const OriginalPrimitives& originalPrimitives, const SpatialSplit& split, PrimInsertIter left, PrimInsertIter right);

}
