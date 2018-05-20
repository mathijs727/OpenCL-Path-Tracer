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
    // Leaf nodes should have at least 3 primitives
    if (primitives.size() < 4)
        return {};

    glm::vec3 extent = nodeBounds.max - nodeBounds.min;
    float currentNodeSAH = nodeBounds.surfaceArea() * primitives.size();

    // For all three axis
    std::optional<SpatialSplit> bestSplit;
    for (int axis : axisToConsider) {
        // Going much further will only cause problems because of numerical inaccuracies
        //if (nodeBounds.size()[axis] <= 0.001f)
        //    continue;

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
                //float position = nodeBounds.min[axis] + splitPosition * (extent[axis] / BVH_SPATIAL_BIN_COUNT);
                assert(mergedLeftBins.rightPlane == mergedRightBins.leftPlane);
                float position = mergedLeftBins.rightPlane;
                bestSplit = SpatialSplit{
                    axis,
                    position,
                    enterCount,
                    exitCount,
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
    int leftCount = 0, rightCount = 0;
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
            //assert(leftPrimBoundsOpt || rightPrimBoundsOpt);
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

    if (leftCount == 0 || rightCount == 0) {
        std::cout << "Real: left: " << leftCount << "; right: " << rightCount << "\n";
        std::cout << "Expected: left: " << split.left << "; right: " << split.right << "\n";
        std::cout << "Split plane pos: " << split.position << "\n\n";
        for (const PrimitiveData& primitive : primitives) {
            std::cout << "Min/max: (" << primitive.bounds.min[split.axis] << ", " << primitive.bounds.max[split.axis] << ")\n";
        }
        std::cout << std::flush;
        throw std::runtime_error("Illegal split");
    } // else if (leftCount + rightCount == primitives.size() * 2) {
    //   throw std::runtime_error("Split causes too much duplication");
    //}

    // Infinintely thin bounds (caused by axis aligned primitives)
    //assert(leftBounds.surfaceArea() > 0.0f && rightBounds.surfaceArea() > 0.0f);

    return { leftBounds, rightBounds };
}

static std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> performSpatialBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives, const OriginalPrimitives& triangleData)
{
    glm::vec3 extent = nodeBounds.max - nodeBounds.min;

    float k1 = BVH_SPATIAL_BIN_COUNT / extent[axis];
    float k1Inv = extent[axis] / BVH_SPATIAL_BIN_COUNT;

    // Store side planes so we can compare with them without having to worry about floating point drift when recomputing them.
    std::array<SpatialBin, BVH_SPATIAL_BIN_COUNT> bins;
    for (size_t binID = 0; binID < BVH_SPATIAL_BIN_COUNT; binID++) {
        bins[binID].leftPlane = binID == 0 ? nodeBounds.min[axis] : nodeBounds.min[axis] + binID * k1Inv;
        bins[binID].rightPlane = binID == BVH_SPATIAL_BIN_COUNT - 1 ? nodeBounds.max[axis] : nodeBounds.min[axis] + (binID + 1) * k1Inv;
    }

    // Loop through the triangles and calculate bin dimensions and primitive counts
    for (const auto& primitive : primitives) {
        // Calculate the index of the left-most and right-most bins that the primitives covers
        float xMin = k1 * (primitive.bounds.min[axis] - nodeBounds.min[axis]);
        float xMax = k1 * (primitive.bounds.max[axis] - nodeBounds.min[axis]);
        size_t leftBinID = std::min(static_cast<size_t>(xMin), BVH_SPATIAL_BIN_COUNT - 1); // Prevent out of bounds (if centroid on the right bound)
        size_t rightBinID = std::min(static_cast<size_t>(xMax), BVH_SPATIAL_BIN_COUNT - 1); // Prevent out of bounds (if centroid on the right bound)

        // Check against the bins left and right bounds to compensate for floating point drift
        // If the left (min) side of the primitive bounds lies precisely on the splitting plane than it is assigned to the bin right of the plane.
        // If the right (max) side of the primitive bounds lies precisely on the splitting plane than it is assigned to the bin left of the plane.
        while (primitive.bounds.min[axis] <= bins[leftBinID].leftPlane && leftBinID > 0)
            leftBinID--;
        while (primitive.bounds.min[axis] > bins[leftBinID].rightPlane && leftBinID != BVH_SPATIAL_BIN_COUNT - 1)
            leftBinID++;
        while (primitive.bounds.max[axis] < bins[rightBinID].leftPlane && rightBinID > 0)
            rightBinID--;
        while (primitive.bounds.max[axis] >= bins[rightBinID].rightPlane && rightBinID != BVH_SPATIAL_BIN_COUNT - 1)
            rightBinID++;

        // If the primitive lies precisely on the splitting plane than left bin < right bin
        if (leftBinID > rightBinID)
            rightBinID = leftBinID;

        assert(nodeBounds.fullyContains(primitive.bounds));
        assert(leftBinID <= rightBinID);

        /*if (leftBinID == rightBinID) {
            // Triangle is completely contained in 1 bin
            bins[leftBinID].enter++;
            bins[rightBinID].exit++;

            bins[leftBinID].bounds.fit(primitive.bounds);
        } else */
        {
            // Keep track of the actual bins. This may defer from the precalculated ones when the bounds clipping
            // fails (because of floating point errors or zero area bounds).
            int actualLeftBin = BVH_SPATIAL_BIN_COUNT;
            int actualRightBin = -1;

            if (!nodeBounds.fullyContains(primitive.bounds)) {
                throw std::runtime_error("Primitive bounds > node bounds");
            }

            // For each bin covered: clip triangle use it to expand bin bounds
            TriangleSceneData triangle = triangleData.triangles[primitive.globalIndex];
            glm::vec3 v1 = triangleData.vertices[triangle.indices[0]].vertex;
            glm::vec3 v2 = triangleData.vertices[triangle.indices[1]].vertex;
            glm::vec3 v3 = triangleData.vertices[triangle.indices[2]].vertex;
            bool assigned = false;
            for (int binID = leftBinID; binID <= rightBinID; binID++) {
                AABB binBounds = primitive.bounds;
                binBounds.min[axis] = bins[binID].leftPlane;
                binBounds.max[axis] = bins[binID].rightPlane;

                auto clippedPrimBoundsOpt = clipTriangleBounds(binBounds, v1, v2, v3);
                if (clippedPrimBoundsOpt) {
                    actualLeftBin = std::min(actualLeftBin, binID);
                    actualRightBin = std::max(actualRightBin, binID);
                    bins[binID].bounds.fit(*clippedPrimBoundsOpt);
                    assigned = true;
                }
            }
            //assert(assigned);

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
    if (intersectPos >= 0.0f && intersectPos <= 1.0f) {
        glm::vec3 intersection = start + intersectPos * edge;
        intersection[axis] = planePos;
        return intersection;
    } else {
        return {};
    }
}

static std::optional<AABB> clipTriangleBounds(AABB bounds, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3)
{
    eastl::fixed_vector<glm::vec3, 11> v = { v1, v2, v3 };
    eastl::fixed_vector<glm::vec3, 11> newV;
    // every segment is represented as v[i] + e[i] * t. We find t.
    for (int axis = 0; axis < 3; ++axis)
        for (int p = 0; p < 2; ++p) {
            bool minNotMax = p == 0;
            glm::vec3 plane_normal(0);
            plane_normal[axis] = 1.f;
            std::vector<bool> insideV(v.size());
            float left = bounds.min[axis];
            float right = bounds.max[axis];
            for (size_t i = 0; i < v.size(); ++i) {
                if (minNotMax) {
                    insideV[i] = v[i][axis] >= left;
                } else {
                    insideV[i] = v[i][axis] <= right;
                }
            }
            for (size_t i = 0; i < v.size(); ++i) {
                int prevI = (i == 0) ? (v.size() - 1) : (i - 1);
                if ((insideV[i] && !insideV[prevI]) || (insideV[prevI] && !insideV[i])) {
                    auto e = v[i] - v[prevI];
                    // do line plane intersection
                    float mag = glm::length(e);
                    auto normal = e / mag;
                    float denom = glm::dot(plane_normal, normal);
                    if (abs(denom) > 0) {
                        float offset = minNotMax ? left : right;
                        float t = -(v[i][axis] - offset) / denom;
                        auto new_v = v[i] + normal * t;
                        //assert(abs(new_v[axis] - offset) < 0.0001f);
                        newV.push_back(new_v);
                    }
                }
                if (insideV[i]) {
                    newV.push_back(v[i]);
                }
            }
            v = std::move(newV);
            newV.clear();
        }
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max()); // Work around bug in glm
    glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest()); // Work around bug in glm
    //assert(v.size() >= 3);
    if (v.size() < 3)
        return {};

    for (size_t i = 0; i < v.size(); ++i) {
        for (int ax = 0; ax < 3; ++ax) {
            min[ax] = glm::min(min[ax], v[i][ax]);
            max[ax] = glm::max(max[ax], v[i][ax]);
        }
    }
    AABB triangleBounds;
    triangleBounds.min = min;
    triangleBounds.max = max;
    return triangleBounds;
}

/*static std::optional<AABB> clipTriangleBounds(AABB bounds, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3)
{
    // Vertices ordered such that the current and next vertex share an edge
    // Every half plane might "cut off" one vertex and replace it by two vertices.
    // So the maximum number of vertices is 3 + 8 = 11
    eastl::fixed_vector<glm::vec3, 11> orderedVertices = { v1, v2, v3 };
    //std::vector<glm::vec3> orderedVertices = { v1, v2, v3 };

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
            //std::vector<glm::vec3> newVertices;
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

    if (orderedVertices.size() < 3)
        return {};

    AABB outBounds;
    for (auto vertex : orderedVertices)
        outBounds.fit(vertex);
    return outBounds;
}*/
}
