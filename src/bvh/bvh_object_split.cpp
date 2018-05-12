#include "bvh_object_split.h"
#include "bvh_build.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <optional>

namespace raytracer {

static constexpr size_t BVH_OBJECT_BIN_COUNT = 32;
struct ObjectBin {
    size_t primCount = 0;
    AABB bounds;
};

static std::array<ObjectBin, BVH_OBJECT_BIN_COUNT> performObjectBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives);

std::optional<ObjectSplit> findObjectSplitBinned(const SubBvhNode& node, gsl::span<const PrimitiveData> primitives, gsl::span<const int> axisToConsider)
{
    // Leaf nodes should have at least 3 primitives
    if (primitives.size() < 4)
        return {};

    glm::vec3 extent = node.bounds.max - node.bounds.min;
    float currentNodeSAH = node.bounds.surfaceArea() * primitives.size();

    // For all three axis
    std::optional<ObjectSplit> bestObjectSplit;
    for (int axis : axisToConsider) {
        if (extent[axis] < 0.001f) // Skip if the bounds along this axis is too small
            continue;

        // Loop through the triangles and calculate bin dimensions and primitive counts
        auto bins = performObjectBinning(node.bounds, axis, primitives);

        // Calculate the cummulative surface area (SAH left + right of split) for all possible splits
        for (size_t binID = 1; binID < BVH_OBJECT_BIN_COUNT; binID++) {
            auto mergedLeftBins = std::accumulate(bins.begin(), bins.begin() + binID, ObjectBin(), [](const auto& left, const auto& right) -> ObjectBin {
                return { left.primCount + right.primCount, left.bounds + right.bounds };
            });

            auto mergedRightBins = std::accumulate(bins.begin() + binID, bins.end(), ObjectBin(), [](const auto& left, const auto& right) -> ObjectBin {
                return { left.primCount + right.primCount, left.bounds + right.bounds };
            });

            // If all primitive centers lie on one side of the splitting plane then the split is invalid
            if (mergedLeftBins.primCount == 0 || mergedRightBins.primCount == 0)
                continue;

            // SAH: Surface Area Heuristic
            float sah = mergedLeftBins.primCount * mergedLeftBins.bounds.surfaceArea() + mergedRightBins.primCount * mergedRightBins.bounds.surfaceArea();
            if (!bestObjectSplit || (sah < bestObjectSplit->sah && sah < currentNodeSAH)) { // Lower surface area heuristic is better
                float position = node.bounds.min[axis] + binID * (extent[axis] / BVH_OBJECT_BIN_COUNT);
                bestObjectSplit = ObjectSplit{
                    axis,
                    position,
                    mergedLeftBins.bounds,
                    mergedRightBins.bounds,
                    sah
                };
            }
        }
    }

    if (bestObjectSplit)
        return bestObjectSplit;
    else
        return {};
}

void performObjectSplit(gsl::span<const PrimitiveData> primitives, const ObjectSplit& split, PrimInsertIter left, PrimInsertIter right)
{
    std::partition_copy(primitives.begin(), primitives.end(), left, right, [&](const PrimitiveData& primitive) -> bool {
        return primitive.bounds.center()[split.axis] < split.position;
    });
}

size_t performObjectSplitInPlace(gsl::span<PrimitiveData> primitives, const ObjectSplit& split)
{
    auto iter = std::partition(primitives.begin(), primitives.end(), [&](const PrimitiveData& primitive) {
        return primitive.bounds.center()[split.axis] < split.position;
    });
    return std::distance(primitives.begin(), iter);
}

static std::array<ObjectBin, BVH_OBJECT_BIN_COUNT> performObjectBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives)
{
    glm::vec3 extent = nodeBounds.max - nodeBounds.min;

    // Loop through the triangles and calculate bin dimensions and primitive counts
    std::array<ObjectBin, BVH_OBJECT_BIN_COUNT> bins;
    float k1 = BVH_OBJECT_BIN_COUNT / extent[axis];
    float k1Inv = extent[axis] / BVH_OBJECT_BIN_COUNT;
    for (const auto& primitive : primitives) {
        // Calculate the bin ID as described in the paper
        float primCenter = primitive.bounds.center()[axis];
        float x = k1 * (primCenter - nodeBounds.min[axis]);
        size_t binID = std::min(static_cast<size_t>(x), BVH_OBJECT_BIN_COUNT - 1); // Prevent out of bounds (if centroid on the right bound)

        // Check against the bins left and right bounds to compensate for floating point drift
        float leftBound = nodeBounds.min[axis] + binID * k1Inv;
        float rightBound = nodeBounds.min[axis] + (binID + 1) * k1Inv;
        if (primCenter < leftBound)
            binID--;
        else if (primCenter >= rightBound)
            binID++;

        auto& bin = bins[binID];
        bin.primCount++;
        bin.bounds.fit(primitive.bounds);
    }

    return bins;
}
}
