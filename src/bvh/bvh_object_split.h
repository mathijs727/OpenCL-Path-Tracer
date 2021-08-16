#pragma once
#include "aabb.h"
#include "bvh_nodes.h"
#include "opencl/cl_helpers.h"
#include <span>
#include <limits>
#include <optional>

namespace raytracer {

struct ObjectSplit {
    int axis;
    float position;

    AABB leftBounds;
    AABB rightBounds;
    float partialSAH = std::numeric_limits<float>::max();
};

std::optional<ObjectSplit> findObjectSplitBinned(
    const AABB& nodeBounds,
    std::span<const PrimitiveData> primitives,
    const OriginalPrimitives& orignalPrimitives,
    std::span<const int> axisToConsider);
std::pair<AABB, AABB> performObjectSplit(std::span<const PrimitiveData> primitives, const OriginalPrimitives&, const ObjectSplit& split, PrimInsertIter left, PrimInsertIter right);
size_t performObjectSplitInPlace(std::span<PrimitiveData> primitives, const ObjectSplit& split);

}
