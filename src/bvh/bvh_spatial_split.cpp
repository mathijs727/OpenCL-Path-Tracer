#include "bvh_spatial_split.h"
#include <algorithm>
#include <array>
#include <eastl/fixed_vector.h>
#include <optional>
#include <tuple>

namespace raytracer {

static constexpr size_t BVH_SPATIAL_BIN_COUNT = 8;
static constexpr bool HIGH_QUALITY_CLIPS = true;
static constexpr bool UNSPLITTING = true;

struct SpatialBin {
    AABB bounds;
    size_t enter = 0;
    size_t exit = 0;

    SpatialBin operator+(const SpatialBin& other) const
    {
        return { bounds + other.bounds, enter + other.enter, exit + other.exit };
    }
};

static std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> performSpatialBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives, const OriginalPrimitives& originalPrimitives);
static AABB clipTriangleBounds(AABB bounds, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3);

std::optional<SpatialSplit> findSpatialSplitBinned(const AABB& nodeBounds, gsl::span<const PrimitiveData> primitives, const OriginalPrimitives& originalPrimitives, gsl::span<const int> axisToConsider)
{
    // Leaf nodes should have at least 3 primitives
    if (primitives.size() < 4)
        return {};

    glm::vec3 extent = nodeBounds.max - nodeBounds.min;
    float currentNodeSAH = nodeBounds.surfaceArea() * primitives.size();

    // For all three axis
    std::optional<SpatialSplit> bestSplit;
    for (int axis : axisToConsider) {
        // Build a histogram based on the position of the bound centers along the given axis
        auto bins = performSpatialBinning(nodeBounds, axis, primitives, originalPrimitives);

        // Combine bins from left-to-right (summedBins) and right-to-left (inverseSummedBins)
        std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> summedBins;
        std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> inverseSummedBins;
        std::exclusive_scan(bins.begin(), bins.end(), summedBins.begin(), SpatialBin{});
        std::exclusive_scan(bins.rbegin(), bins.rend(), inverseSummedBins.begin(), SpatialBin{});
        for (int splitPosition = 1; splitPosition < BVH_SPATIAL_BIN_COUNT; splitPosition++) {
            // Get bounds/primitive counts at the left and right of the split plane
            SpatialBin mergedLeftBins = summedBins[splitPosition];
            SpatialBin mergedRightBins = inverseSummedBins[BVH_SPATIAL_BIN_COUNT - splitPosition];

            size_t enterCount = mergedLeftBins.enter;
            size_t exitCount = mergedRightBins.exit;

            // Ignore splits that have 0 primitives on either side
            if (enterCount == 0 || exitCount == 0)
                continue;

            // SAH: Surface Area Heuristic
            float sah = enterCount * mergedLeftBins.bounds.surfaceArea() + exitCount * mergedRightBins.bounds.surfaceArea();
            if ((!bestSplit || sah < bestSplit->sah) && sah < currentNodeSAH) { // Lower surface area heuristic is better
                float position = nodeBounds.min[axis] + splitPosition * (extent[axis] / BVH_SPATIAL_BIN_COUNT);
                bestSplit = SpatialSplit{
                    axis,
                    position,
                    mergedLeftBins.bounds,
                    mergedRightBins.bounds,
                    sah
                };
            }
        }
    }

    if (bestSplit)
        return bestSplit;
    else
        return {};
}

std::pair<AABB, AABB> performSpatialSplit(gsl::span<const PrimitiveData> primitives, const OriginalPrimitives& originalPrimitives, const SpatialSplit& split, PrimInsertIter left, PrimInsertIter right)
{
    AABB leftBounds, rightBounds;
    for (const PrimitiveData& primitive : primitives) {
        float min = primitive.bounds.min[split.axis];
        float max = primitive.bounds.max[split.axis];

        if (min < split.position && max > split.position) {
            // Primitive getting split by the plane
            auto triangle = originalPrimitives.triangles[primitive.globalIndex];
            glm::vec3 v1 = originalPrimitives.vertices[triangle.indices[0]].vertex;
            glm::vec3 v2 = originalPrimitives.vertices[triangle.indices[1]].vertex;
            glm::vec3 v3 = originalPrimitives.vertices[triangle.indices[2]].vertex;

            // Split the triangle (primitives) bounds by the splitting plane
            AABB leftClipBounds = primitive.bounds;
            AABB rightClipBounds = primitive.bounds;
            leftClipBounds.max[split.axis] = split.position;
            rightClipBounds.min[split.axis] = split.position;
            auto leftPrimBounds = clipTriangleBounds(leftClipBounds, v1, v2, v3);
            auto rightPrimBounds = clipTriangleBounds(rightClipBounds, v1, v2, v3);

            leftBounds.fit(leftPrimBounds);
            rightBounds.fit(rightPrimBounds);
            *left++ = PrimitiveData{ primitive.globalIndex, leftPrimBounds };
            *right++ = PrimitiveData{ primitive.globalIndex, rightPrimBounds };
        } else if (max < split.position) {
            // Primitive fully to the left of the split plane
            leftBounds.fit(primitive.bounds);
            *left++ = primitive;
        } else {
            // Primitive fully to the right of the split plane
            rightBounds.fit(primitive.bounds);
            *right++ = primitive;
        }
    }

    return { leftBounds, rightBounds };
}

static std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> performSpatialBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives, const OriginalPrimitives& triangleData)
{
    glm::vec3 extent = nodeBounds.max - nodeBounds.min;

    // Loop through the triangles and calculate bin dimensions and primitive counts
    std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> bins;
    float k1 = BVH_SPATIAL_BIN_COUNT / extent[axis];
    float k1Inv = extent[axis] / BVH_SPATIAL_BIN_COUNT;
    for (const auto& primitive : primitives) {
        // Calculate the index of the left-most and right-most bins that the primitives covers
        float xMin = k1 * (primitive.bounds.min[axis] - nodeBounds.min[axis]);
        float xMax = k1 * (primitive.bounds.max[axis] - nodeBounds.min[axis]);
        size_t leftBinID = std::min(static_cast<size_t>(xMin), BVH_SPATIAL_BIN_COUNT - 1); // Prevent out of bounds (if centroid on the right bound)
        size_t rightBinID = std::min(static_cast<size_t>(xMax), BVH_SPATIAL_BIN_COUNT - 1); // Prevent out of bounds (if centroid on the right bound)

        assert(nodeBounds.fullyContains(primitive.bounds));

        bins[leftBinID].enter++;
        bins[rightBinID].exit++;

        if (leftBinID == rightBinID) {
            // Triangle is completely contained in 1 bin
            bins[leftBinID].bounds.fit(primitive.bounds);
        } else {
            // For each bin covered: clip triangle use it to expand bin bounds
            TriangleSceneData triangle = triangleData.triangles[primitive.globalIndex];
            glm::vec3 v1 = triangleData.vertices[triangle.indices[0]].vertex;
            glm::vec3 v2 = triangleData.vertices[triangle.indices[1]].vertex;
            glm::vec3 v3 = triangleData.vertices[triangle.indices[2]].vertex;
            for (size_t binID = leftBinID; binID <= rightBinID; binID++) {
                float leftPlane = nodeBounds.min[axis] + binID * k1Inv;
                float rightPlane = std::min(nodeBounds.min[axis] + (binID + 1) * k1Inv, nodeBounds.max[axis]); // Prevent floating point drift errors
                AABB binBounds = nodeBounds;
                binBounds.min[axis] = leftPlane;
                binBounds.max[axis] = rightPlane;
                AABB clippedPrimBounds = clipTriangleBounds(binBounds, v1, v2, v3);
                assert(nodeBounds.fullyContains(clippedPrimBounds));
                bins[binID].bounds.fit(clippedPrimBounds);
            }
        }
    }

    return bins;
}

static std::optional<glm::vec3> lineAxisAlignedPlaneIntersection(glm::vec3 v1, glm::vec3 v2, int axis, float planePos)
{
    // Line lies parallel to the plane
    if (v1[axis] == v2[axis])
        return {};

    glm::vec3 start, end;
    if (v1[axis] < planePos) {
        start = v1;
        end = v2;
    } else {
        start = v2;
        end = v1;
    }

    glm::vec3 edge = end - start;
    float intersectPos = (planePos - start[axis]) / edge[axis];
    if (intersectPos >= 0.0f && intersectPos <= 1.0f) {
        return start + intersectPos * edge;
    } else {
        return {};
    }
}

static AABB clipTriangleBounds(AABB bounds, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3)
{
    // Vertices ordered such that the current and next vertex share an edge
    // Every half plane might "cut off" one vertex and replace it by two vertices.
    // So the maximum number of vertices is 3 + 8 = 11
    eastl::fixed_vector<glm::vec3, 11> orderedVertices = { v1, v2, v3 };

    // For each axis
    for (int axis = 0; axis < 3; axis++) {
        // For each side of the bounding box along the axis
        for (size_t p = 0; p < 2; p++) {
            // Define axis-aligned half plane
            float planePos = (p == 0 ? bounds.min[axis] : bounds.max[axis]);
            float planeNormalDirection = (p == 0 ? 1.0f : -1.0f);
            glm::vec3 planeNormal = glm::vec3(0);
            planeNormal[axis] = planeNormalDirection;

            // Loop through all the vertices; keep vertices that are inside the half plane and add new vertices at intersections between edges and the half plane
            eastl::fixed_vector<glm::vec3, 11> newVertices;
            for (int i = 0; i < orderedVertices.size(); i++) {
                glm::vec3 currentVertex = orderedVertices[i];
                glm::vec3 nextVertex = orderedVertices[(i + 1) % orderedVertices.size()];

                // Check whether the current and next node are on the correct side of the half plane
                bool containsCurrent = (currentVertex[axis] - planePos) * planeNormalDirection >= 0.0f;
                bool containsNext = (nextVertex[axis] - planePos) * planeNormalDirection >= 0.0f;

                if (containsCurrent)
                    newVertices.push_back(currentVertex);

                if (containsCurrent != containsNext) { // Going in / out of the plane
                    auto intersectionOpt = lineAxisAlignedPlaneIntersection(currentVertex, nextVertex, axis, planePos);
                    assert(intersectionOpt); // Should always find an intersection
                    newVertices.push_back(*intersectionOpt);
                }
            }
            orderedVertices = std::move(newVertices);
        }
    }

    AABB outBounds;
    for (auto vertex : orderedVertices)
        outBounds.fit(vertex);
    return outBounds;
}

}
