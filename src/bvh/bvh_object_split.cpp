#include "bvh_object_split.h"
#include "bvh_build.h"
#include <algorithm>
#include <array>
#include <limits>
#include <numeric>

namespace raytracer {

static constexpr int BVH_OBJECT_BIN_COUNT = 32;

struct ObjectBin {
    size_t primCount = 0;
    AABB bounds;
    float leftPlane = std::numeric_limits<float>::max();
    float rightPlane = std::numeric_limits<float>::lowest();

    ObjectBin operator+(const ObjectBin& other) const
    {
        return { primCount + other.primCount, bounds + other.bounds, std::min(leftPlane, other.leftPlane), std::max(rightPlane, other.rightPlane) };
    }
};

static std::array<ObjectBin, BVH_OBJECT_BIN_COUNT> performObjectBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives);

std::optional<ObjectSplit> findObjectSplitBinned(const AABB& nodeBounds, gsl::span<const PrimitiveData> primitives, const OriginalPrimitives&, gsl::span<const int> axisToConsider)
{
    glm::vec3 extent = nodeBounds.extent();

    // For all three axis
    std::optional<ObjectSplit> bestSplit;
    for (int axis : axisToConsider) {
        if (extent[axis] <= std::numeric_limits<float>::min())
            continue;

        // Build a histogram based on the position of the bound centers along the given axis
        auto bins = performObjectBinning(nodeBounds, axis, primitives);

        // Combine bins from left-to-right (summedBins) and right-to-left (inverseSummedBins)
        std::array<ObjectBin, BVH_OBJECT_BIN_COUNT> summedBins;
        std::array<ObjectBin, BVH_OBJECT_BIN_COUNT> inverseSummedBins;
        //std::exclusive_scan(bins.begin(), bins.end(), summedBins.begin(), ObjectBin{});
        //std::exclusive_scan(bins.rbegin(), bins.rend(), inverseSummedBins.begin(), ObjectBin{});
        std::partial_sum(bins.begin(), bins.end(), summedBins.begin());
        std::partial_sum(bins.rbegin(), bins.rend(), inverseSummedBins.begin());
        for (int splitPosition = 1; splitPosition < BVH_OBJECT_BIN_COUNT; splitPosition++) {
            // Get bounds/primitive counts at the left and right of the split plane
            //ObjectBin mergedLeftBins = summedBins[splitPosition];
            //ObjectBin mergedRightBins = inverseSummedBins[BVH_OBJECT_BIN_COUNT - splitPosition];
            ObjectBin mergedLeftBins = summedBins[splitPosition - 1];
            ObjectBin mergedRightBins = inverseSummedBins[BVH_OBJECT_BIN_COUNT - splitPosition - 1];

            // If all primitive centers lie on one side of the splitting plane then the split is invalid
            if (mergedLeftBins.primCount == 0 || mergedRightBins.primCount == 0)
                continue;

            // SAH: Surface Area Heuristic
            float partialSAH = mergedLeftBins.primCount * mergedLeftBins.bounds.surfaceArea() + mergedRightBins.primCount * mergedRightBins.bounds.surfaceArea();
            if (!bestSplit || partialSAH < bestSplit->partialSAH) { // Lower surface area heuristic is better
                assert(mergedLeftBins.rightPlane == mergedRightBins.leftPlane);
                float position = mergedLeftBins.rightPlane;
                bestSplit = ObjectSplit{
                    axis,
                    position,
                    mergedLeftBins.bounds,
                    mergedRightBins.bounds,
                    partialSAH
                };
            }
        }
    }

    return bestSplit;
}

std::pair<AABB, AABB> performObjectSplit(gsl::span<const PrimitiveData> primitives, const OriginalPrimitives&, const ObjectSplit& split, PrimInsertIter left, PrimInsertIter right)
{
    std::partition_copy(primitives.begin(), primitives.end(), left, right, [&](const PrimitiveData& primitive) -> bool {
        return primitive.bounds.center()[split.axis] < split.position;
    });
    return { split.leftBounds, split.rightBounds };
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

    float k1 = BVH_OBJECT_BIN_COUNT / extent[axis];
    float k1Inv = extent[axis] / BVH_OBJECT_BIN_COUNT;

    // Store side planes so we can compare with them without having to worry about floating point drift when recomputing them.
    std::array<ObjectBin, BVH_OBJECT_BIN_COUNT> bins;
    for (size_t binID = 0; binID < BVH_OBJECT_BIN_COUNT; binID++) {
        bins[binID].leftPlane = binID == 0 ? nodeBounds.min[axis] : nodeBounds.min[axis] + binID * k1Inv;
        bins[binID].rightPlane = binID == BVH_OBJECT_BIN_COUNT - 1 ? nodeBounds.max[axis] : nodeBounds.min[axis] + (binID + 1) * k1Inv;
    }

    // Loop through the triangles and calculate bin dimensions and primitive counts
    for (const auto& primitive : primitives) {
        // Calculate the bin ID as described in the paper
        float primCenter = primitive.bounds.center()[axis];
        float x = k1 * (primCenter - nodeBounds.min[axis]);
        size_t binID = std::min(static_cast<int>(x), BVH_OBJECT_BIN_COUNT - 1); // Prevent out of bounds (if centroid on the right bound)

        // Check against the bins left and right bounds to compensate for floating point drift
        // Left (min) bound is inclusive, right (max) bound is exclusve
        while (primCenter < bins[binID].leftPlane)
            binID--;
        while (primCenter >= bins[binID].rightPlane && binID != BVH_OBJECT_BIN_COUNT - 1)
            binID++;

        auto& bin = bins[binID];
        bin.primCount++;
        bin.bounds.fit(primitive.bounds);
    }

    return bins;
}
}
