#pragma once
#include "aabb.h"
#include "bvh_nodes.h"
#include "opencl/cl_helpers.h"
#include <gsl/gsl>
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
    gsl::span<const PrimitiveData> primitives,
    const OriginalPrimitives& orignalPrimitives,
    gsl::span<const int> axisToConsider = std::array{ 0, 1, 2 });
std::pair<AABB, AABB> performObjectSplit(gsl::span<const PrimitiveData> primitives, const OriginalPrimitives&, const ObjectSplit& split, PrimInsertIter left, PrimInsertIter right);
size_t performObjectSplitInPlace(gsl::span<PrimitiveData> primitives, const ObjectSplit& split);

}
