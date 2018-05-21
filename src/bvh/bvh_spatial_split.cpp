#include "bvh_spatial_split.h"
#include <algorithm>
#include <array>
#include <eastl/fixed_vector.h>
#include <optional>
#include <tuple>

namespace raytracer {

static constexpr int BVH_SPATIAL_BIN_COUNT = 8;
static constexpr bool HIGH_QUALITY_CLIPS = true;
static constexpr bool UNSPLITTING = true;

struct SpatialBin {
    size_t enter = 0;
    size_t exit = 0;
    AABB bounds;
    float leftPlane = std::numeric_limits<float>::max();
    float rightPlane = std::numeric_limits<float>::lowest();

    SpatialBin operator+(const SpatialBin& other) const
    {
        return { enter + other.enter, exit + other.exit, bounds + other.bounds, std::min(leftPlane, other.leftPlane), std::max(rightPlane, other.rightPlane) };
    }
};

static std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> performSpatialBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives, const OriginalPrimitives& originalPrimitives);
static std::optional<AABB> clipTriangleBounds(AABB bounds, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3);

std::optional<SpatialSplit> findSpatialSplitBinned(const AABB& nodeBounds, gsl::span<const PrimitiveData> primitives, const OriginalPrimitives& originalPrimitives, gsl::span<const int> axisToConsider)
{
    glm::vec3 extent = nodeBounds.extent();

    // For all three axis
    std::optional<SpatialSplit> bestSplit;
    for (int axis : axisToConsider) {
        // Going much further will only cause problems because of numerical inaccuracies
        if (extent[axis] <= std::numeric_limits<float>::min())
            continue;

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
            float partialSAH = enterCount * mergedLeftBins.bounds.surfaceArea() + exitCount * mergedRightBins.bounds.surfaceArea();
            if (!bestSplit || partialSAH < bestSplit->partialSAH) { // Lower surface area heuristic is better
                assert(mergedLeftBins.rightPlane == mergedRightBins.leftPlane);
                float position = mergedLeftBins.rightPlane;
                bestSplit = SpatialSplit{
                    axis,
                    position,
                    enterCount,
                    exitCount,
                    mergedLeftBins.bounds,
                    mergedRightBins.bounds,
                    partialSAH
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
    // http://www.nvidia.com/docs/IO/77714/sbvh.pdf
    // Reference unsplitting
    float splitCost = split.leftBounds.surfaceArea() * split.leftCount + split.rightBounds.surfaceArea() * split.rightCount;

    AABB leftBounds, rightBounds;
    int leftCount = 0, rightCount = 0;
    for (const PrimitiveData& primitive : primitives) {
        float min = primitive.bounds.min[split.axis];
        float max = primitive.bounds.max[split.axis];

        if (min < split.position && max > split.position) {

            // Reference unsplitting
            float cost1 = (split.leftBounds + primitive.bounds).surfaceArea() * split.leftCount + split.rightBounds.surfaceArea() * (split.rightCount - 1);
            float cost2 = split.leftBounds.surfaceArea() * (split.leftCount - 1) + (split.rightBounds + primitive.bounds).surfaceArea() * split.rightCount;
            float optimalUnsplitCost = std::min(cost1, cost2);
            if (optimalUnsplitCost < splitCost) { // Unsplitting
                if (cost1 < cost2) {
                    // Left
                    leftBounds.fit(primitive.bounds);
                    *left++ = primitive;
                    leftCount++;
                } else {
                    // Right
                    rightBounds.fit(primitive.bounds);
                    *right++ = primitive;
                    rightCount++;
                }
            } else { // Splitting
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

                auto leftPrimBoundsOpt = clipTriangleBounds(leftClipBounds, v1, v2, v3);
                auto rightPrimBoundsOpt = clipTriangleBounds(rightClipBounds, v1, v2, v3);

                if (leftPrimBoundsOpt) {
                    leftBounds.fit(*leftPrimBoundsOpt);
                    *left++ = PrimitiveData{ primitive.globalIndex, *leftPrimBoundsOpt };
                    leftCount++;
                }

                if (rightPrimBoundsOpt) {
                    rightBounds.fit(*rightPrimBoundsOpt);
                    *right++ = PrimitiveData{ primitive.globalIndex, *rightPrimBoundsOpt };
                    rightCount++;
                }
            }
        } else if (max < split.position) {
            // Primitive fully to the left of the split plane
            leftBounds.fit(primitive.bounds);
            *left++ = primitive;
            leftCount++;

        } else {
            // Primitive fully to the right of the split plane
            rightBounds.fit(primitive.bounds);
            *right++ = primitive;
            rightCount++;
        }
    }

    if (leftCount == 0 || rightCount == 0)
        throw std::runtime_error("Illegal split");

    return { leftBounds, rightBounds };
}

static std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> performSpatialBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives, const OriginalPrimitives& triangleData)
{
    glm::vec3 extent = nodeBounds.max - nodeBounds.min;

    float k1 = BVH_SPATIAL_BIN_COUNT / extent[axis];
    float k1Inv = extent[axis] / BVH_SPATIAL_BIN_COUNT;

    // Store side planes so we can compare with them without having to worry about floating point drift when recomputing them.
    std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> bins;
    for (int binID = 0; binID < BVH_SPATIAL_BIN_COUNT; binID++) {
        bins[binID].leftPlane = binID == 0 ? nodeBounds.min[axis] : nodeBounds.min[axis] + binID * k1Inv;
        bins[binID].rightPlane = binID == BVH_SPATIAL_BIN_COUNT - 1 ? nodeBounds.max[axis] : nodeBounds.min[axis] + (binID + 1) * k1Inv;
    }

    // Loop through the triangles and calculate bin dimensions and primitive counts
    for (const auto& primitive : primitives) {
        // Calculate the index of the left-most and right-most bins that the primitives covers
        float xMin = k1 * (primitive.bounds.min[axis] - nodeBounds.min[axis]);
        float xMax = k1 * (primitive.bounds.max[axis] - nodeBounds.min[axis]);
        int leftBinID = std::min(static_cast<int>(xMin), BVH_SPATIAL_BIN_COUNT - 1); // Prevent out of bounds (if centroid on the right bound)
        int rightBinID = std::min(static_cast<int>(xMax), BVH_SPATIAL_BIN_COUNT - 1); // Prevent out of bounds (if centroid on the right bound)

        // Check against the bins left and right bounds to compensate for floating point drift
        // If the left (min) side of the primitive bounds lies precisely on the splitting plane than it is assigned to the bin left of the plane.
        // If the right (max) side of the primitive bounds lies precisely on the splitting plane than it is assigned to the bin right of the plane.
        // This makes sure that the results of binning matches the results of performing splitting of the node (if the split is selected).
        while (primitive.bounds.min[axis] <= bins[leftBinID].leftPlane && leftBinID > 0)
            leftBinID--;
        while (primitive.bounds.min[axis] > bins[leftBinID].rightPlane && leftBinID != BVH_SPATIAL_BIN_COUNT - 1)
            leftBinID++;
        while (primitive.bounds.max[axis] < bins[rightBinID].leftPlane && rightBinID > 0)
            rightBinID--;
        while (primitive.bounds.max[axis] >= bins[rightBinID].rightPlane && rightBinID != BVH_SPATIAL_BIN_COUNT - 1)
            rightBinID++;

        assert(nodeBounds.fullyContains(primitive.bounds));
        assert(leftBinID <= rightBinID);

        if (leftBinID == rightBinID) {
            // Triangle is completely contained in 1 bin
            bins[leftBinID].enter++;
            bins[rightBinID].exit++;

            bins[leftBinID].bounds.fit(primitive.bounds);
        } else {
            // Keep track of the actual bins. This may defer from the values we just calculated if the bounds clipping fails (because of floating point errors).
            int actualLeftBin = BVH_SPATIAL_BIN_COUNT;
            int actualRightBin = -1;

            // For each bin covered: clip triangle use it to expand bin bounds
            TriangleSceneData triangle = triangleData.triangles[primitive.globalIndex];
            glm::vec3 v1 = triangleData.vertices[triangle.indices[0]].vertex;
            glm::vec3 v2 = triangleData.vertices[triangle.indices[1]].vertex;
            glm::vec3 v3 = triangleData.vertices[triangle.indices[2]].vertex;
            for (int binID = leftBinID; binID <= rightBinID; binID++) {
                AABB binBounds = primitive.bounds;
                binBounds.min[axis] = bins[binID].leftPlane;
                binBounds.max[axis] = bins[binID].rightPlane;

                auto clippedPrimBoundsOpt = clipTriangleBounds(binBounds, v1, v2, v3);
                if (clippedPrimBoundsOpt) {
                    actualLeftBin = std::min(actualLeftBin, binID);
                    actualRightBin = std::max(actualRightBin, binID);
                    bins[binID].bounds.fit(*clippedPrimBoundsOpt);
                }
            }

            if (actualLeftBin <= actualRightBin) {
                bins[actualLeftBin].enter++;
                bins[actualRightBin].exit++;
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

    // Sort the two vertices along the axis (start = left, end = right)
    glm::vec3 start, end;
    if (v1[axis] < v2[axis]) {
        start = v1;
        end = v2;
    } else {
        start = v2;
        end = v1;
    }

    glm::vec3 edge = end - start;
    float intersectPos = (planePos - start[axis]) / edge[axis];
    if (intersectPos >= 0.0f && intersectPos <= 1.0f) { // Intersection before / after endpoint of the line
        glm::vec3 intersection = start + intersectPos * edge;
        intersection[axis] = planePos; // Make up for numerical inaccuracies
        return intersection;
    } else {
        return {};
    }
}

static std::optional<AABB> clipTriangleBounds(AABB bounds, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3)
{
    // Vertices ordered such that vertices n and n+1 share an edge (wrapping around, so vertex N-1 and 0 also shared an edge)
    // Worst-case scenario: every half plane might "cut off" one vertex and replace it by two vertices;
    // So the maximum number of vertices is 3 + 8 = 11
    // Use eastl::fixed_vector to prevent dynamic memory allocation (which is very slow)
    eastl::fixed_vector<glm::vec3, 11> orderedVertices = { v1, v2, v3 };

    // For each axis
    for (int axis = 0; axis < 3; axis++) {
        // For each side of the bounding box along the axis
        for (int p = 0; p < 2; p++) {
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
                    if (intersectionOpt)
                        newVertices.push_back(*intersectionOpt);
                    else
                        throw std::runtime_error("Logic error in clipping triangle");
                    assert(intersectionOpt); // Should always find an intersection
                }
            }
            orderedVertices = std::move(newVertices);
        }
    }

    if (orderedVertices.size() < 3)
        return {};

    AABB outBounds;
    for (auto vertex : orderedVertices)
        outBounds.fit(vertex);
    return outBounds;
}
}
