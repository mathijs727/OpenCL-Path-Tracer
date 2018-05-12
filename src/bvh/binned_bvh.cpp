#include "binned_bvh.h"
#include "bvh_build.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <optional>

namespace raytracer {

struct ObjectSplit {
    int axis;
    float position;
};

static std::optional<ObjectSplit> findOptimalObjectSplit(const SubBvhNode& node, gsl::span<const PrimitiveData> primitives);
static void performObjectSplit(gsl::span<const PrimitiveData> primitives, const ObjectSplit& split, PrimInsertIter left, PrimInsertIter right);

std::tuple<uint32_t, std::vector<TriangleSceneData>, std::vector<SubBvhNode>> buildBinnedBVH(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles)
{
    auto primitives = generatePrimitives(vertices, triangles);
    auto [rootNodeID, reorderedPrimitives, bvhNodes] = buildBVH(std::move(primitives), [](const SubBvhNode& node, gsl::span<const PrimitiveData> primitives, PrimInsertIter left, PrimInsertIter right) -> bool {
        if (auto split = findOptimalObjectSplit(node, primitives); split) {
            performObjectSplit(primitives, *split, left, right);
            return true;
        } else {
            std::copy(primitives.begin(), primitives.end(), left);
            return false;
        }
    });

    std::vector<TriangleSceneData> outTriangles(reorderedPrimitives.size());
    std::transform(reorderedPrimitives.begin(), reorderedPrimitives.end(), outTriangles.begin(), [&](const PrimitiveData& primitive) {
        return triangles[primitive.globalIndex];
    });
    return { rootNodeID, outTriangles, bvhNodes };
}

static std::optional<ObjectSplit> findOptimalObjectSplit(const SubBvhNode& node, gsl::span<const PrimitiveData> primitives)
{
    // Leaf nodes should have at least 3 primitives
    if (primitives.size() < 4)
        return {};

    glm::vec3 extent = node.bounds.max - node.bounds.min;
    float currentNodeSAH = node.bounds.surfaceArea() * primitives.size();

    // For all three axis
    struct ObjectSplitInfo {
        ObjectSplit split;

        float sah = std::numeric_limits<float>::max();
        AABB leftBounds;
        AABB rightBounds;
    };
    std::optional<ObjectSplitInfo> bestObjectSplit;
    for (int axis = 0; axis < 3; axis++) {
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

                size_t left = 0;
                size_t right = 0;
                for (const auto& primitive : primitives) {
                    bool isLeft = primitive.bounds.center()[axis] < position;
                    if (isLeft)
                        left++;
                    else
                        right++;
                }
                assert(left == mergedLeftBins.primCount && right == mergedRightBins.primCount);
                assert(left != 0 && right != 0);

                bestObjectSplit = ObjectSplitInfo{
                    { axis, position },
                    sah,
                    mergedLeftBins.bounds,
                    mergedRightBins.bounds
                };
            }
        }
    }

    if (bestObjectSplit)
        return bestObjectSplit->split;
    else
        return {};
}

static void performObjectSplit(gsl::span<const PrimitiveData> primitives, const ObjectSplit& split, PrimInsertIter left, PrimInsertIter right)
{
    std::partition_copy(primitives.begin(), primitives.end(), left, right, [=](const PrimitiveData& primitive) -> bool {
        float primPos = primitive.bounds.center()[split.axis];
        return primPos < split.position;
    });
}
}
