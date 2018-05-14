#include "bvh_spatial_split.h"
#include "fix_variant.h"
#include <algorithm>
#include <array>
#include <optional>
#include <tuple>
#include <variant>

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
//static AABB clipTrianglePlanes(int axis, float leftPlane, float rightPlane, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, const AABB& bounds);
//static std::pair<AABB, AABB> clipTrianglePlane(int axis, float position, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, const AABB& bounds);
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
    size_t leftCount = 0, rightCount = 0;
    for (const PrimitiveData& primitive : primitives) {
        float min = primitive.bounds.min[split.axis];
        float max = primitive.bounds.max[split.axis];

        if (min < split.position && max > split.position) {
            // Primitive getting split by the plane
            auto triangle = originalPrimitives.triangles[primitive.globalIndex];
            glm::vec3 v1 = originalPrimitives.vertices[triangle.indices[0]].vertex;
            glm::vec3 v2 = originalPrimitives.vertices[triangle.indices[1]].vertex;
            glm::vec3 v3 = originalPrimitives.vertices[triangle.indices[2]].vertex;
            AABB leftClipBounds = primitive.bounds;
            AABB rightClipBounds = primitive.bounds;
            leftClipBounds.max[split.axis] = split.position;
            rightClipBounds.min[split.axis] = split.position;

            auto leftPrimBounds = clipTriangleBounds(leftClipBounds, v1, v2, v3);
            auto rightPrimBounds = clipTriangleBounds(rightClipBounds, v1, v2, v3);
            leftBounds.fit(leftPrimBounds);
            rightBounds.fit(rightPrimBounds);
            //assert(split.leftBounds.fullyContains(leftPrimBounds));
            //assert(split.rightBounds.fullyContains(rightBounds));
            *left++ = PrimitiveData{ primitive.globalIndex, leftPrimBounds };
            *right++ = PrimitiveData{ primitive.globalIndex, rightPrimBounds };

            leftCount++;
            rightCount++;
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

    assert(leftCount > 0 && rightCount > 0);
    size_t size = primitives.size();
    assert(leftCount + rightCount >= size);

    return { leftBounds, rightBounds };
    //return { split.leftBounds, split.rightBounds };
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

        /*// Check against the bins left and right bounds to compensate for floating point drift
        float minLeftBound = nodeBounds.min[axis] + leftBinID * k1Inv;
        float minRightBound = nodeBounds.min[axis] + (leftBinID + 1) * k1Inv;
        if (primitive.bounds.min[axis] < minLeftBound)
            leftBinID--;
        else if (primitive.bounds.min[axis] > minRightBound)
            leftBinID++;


        // Check against the bins left and right bounds to compensate for floating point drift
        float maxLeftBound = nodeBounds.min[axis] + rightBinID * k1Inv;
        float maxRightBound = nodeBounds.min[axis] + (rightBinID + 1) * k1Inv;
        if (primitive.bounds.max[axis] < maxLeftBound)
            rightBinID--;
        else if (primitive.bounds.max[axis] > maxRightBound)
            rightBinID++;*/

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
                assert(nodeBounds.fullyContains(binBounds));
                assert(nodeBounds.fullyContains(clippedPrimBounds));
                bins[binID].bounds.fit(clippedPrimBounds);
            }
        }
    }

    return bins;
}

static AABB clipTriangleBounds(AABB bounds, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3)
{
    std::vector<glm::vec3> v = { v1, v2, v3 };
    std::vector<glm::vec3> newV;
    // Every segment is represented as v[i] + e[i] * t. We find t.
    for (uint32_t axis = 0; axis < 3; ++axis)
        for (uint32_t p = 0; p < 2; ++p) {
            bool minNotMax = p == 0;
            glm::vec3 plane_normal(0);
            plane_normal[axis] = 1.f;
            std::vector<uint8_t> insideV(v.size());
            float left = bounds.min[axis];
            float right = bounds.max[axis];
            for (uint32_t i = 0; i < v.size(); ++i) {
                if (minNotMax) {
                    insideV[i] = v[i][axis] >= left;
                } else {
                    insideV[i] = v[i][axis] <= right;
                }
            }
            for (uint32_t i = 0; i < v.size(); ++i) {
                int prevI = (i == 0) ? ((uint32_t)v.size() - 1) : (i - 1);
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
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());
    for (uint32_t i = 0; i < v.size(); ++i)
        for (uint32_t ax = 0; ax < 3; ++ax) {
            min[ax] = glm::min(min[ax], v[i][ax]);
            max[ax] = glm::max(max[ax], v[i][ax]);
        }
    AABB triangleBounds;
    triangleBounds.min = min;
    triangleBounds.max = max;
    return triangleBounds.intersection(bounds); // Prevent floating point drift errors
}

/*static AABB clipTrianglePlanes(int axis, float leftPlane, float rightPlane, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, const AABB& inBounds)
{
    struct Event {
        float position;
        std::optional<glm::vec3> vertex;

        bool operator<(const Event& other) const { return position < other.position; };
    };

    // Dumb array implementation requires extra indentation, messing with clang-format
    std::array<Event, 5> events = { { { v1[axis], v1 },
        { v2[axis], v2 },
        { v3[axis], v3 },
        { leftPlane },
        { rightPlane } } };
    std::sort(events.begin(), events.end());

    // Simple sweep-plane algorithm to build the bounds of the triangle in between the two planes
    AABB bounds;
    std::vector<float> visitedPlanes;
    std::vector<glm::vec3> visitedVertices;
    float prevEvent = std::numeric_limits<float>::lowest();
    for (const auto& ev : events) {
        assert(ev.position >= prevEvent);
        prevEvent = ev.position;

        if (ev.vertex) {
            // Vertex evetn
            glm::vec3 vertex = *ev.vertex;

            //assert(!(visitedVertices.size() == 2 && (visitedPlanes.size() == 0 || visitedPlanes.size() == 2)));

            // Expand bounds if the vertex is in between the two planes
            if (visitedPlanes.size() == 1) {
                bounds.fit(vertex);
            }

            // Find intersection points between the splitting plane(s) and edge(s) to the previously visited vertices
            for (glm::vec3 otherVertex : visitedVertices) {
                glm::vec3 edge = vertex - otherVertex;
                for (float splittingPlane : visitedPlanes) {
                    float intersectPosAlongEdge = (splittingPlane - otherVertex[axis]) / edge[axis];
                    if (intersectPosAlongEdge > 0.0f && intersectPosAlongEdge < 1.0f) {
                        glm::vec3 intersectPoint = otherVertex + intersectPosAlongEdge * edge;
                        bounds.fit(intersectPoint);
                    }
                }
            }

            visitedVertices.push_back(vertex);
        } else {
            // Split plane event
            visitedPlanes.push_back(ev.position);
        }
    }

    assert(visitedVertices.size() == 3);

    AABB triangleBounds;
    triangleBounds.fit(v1);
    triangleBounds.fit(v2);
    triangleBounds.fit(v3);

    assert(triangleBounds.fullyContains(bounds));

    return bounds.intersection(inBounds);
}

std::pair<AABB, AABB> clipTrianglePlane(int axis, float planePosition, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, const AABB& inBounds)
{
    struct Event {
        float position;
        std::optional<glm::vec3> vertex;

        bool operator<(const Event& other) const { return position < other.position; };
    };

    // Dumb array implementation requires extra indentation, messing with clang-format
    std::array<Event, 5> events = { { { v1[axis], v1 },
        { v2[axis], v2 },
        { v3[axis], v3 },
        { planePosition } } };
    std::sort(events.begin(), events.end());

    // Simple sweep-plane algorithm to build the bounds of the triangle in between the two planes
    AABB leftBounds, rightBounds;
    bool passedPlane = false;
    std::vector<glm::vec3> visitedVertices;
    for (const auto& ev : events) {
        if (ev.vertex) {
            // Vertex evetn
            glm::vec3 vertex = *ev.vertex;

            // Expand bounds if the vertex is in between the two planes
            if (passedPlane)
                leftBounds.fit(vertex);
            else
                rightBounds.fit(vertex);

            // Find intersection points between the splitting plane(s) and edge(s) to the previously visited vertices
            for (glm::vec3 otherVertex : visitedVertices) {
                glm::vec3 edge = vertex - otherVertex;
                if (passedPlane) {
                    float intersectPosAlongEdge = (planePosition - otherVertex[axis]) / edge[axis];
                    if (intersectPosAlongEdge > 0.0f && intersectPosAlongEdge < 1.0f) {
                        glm::vec3 intersectPoint = otherVertex + intersectPosAlongEdge * edge;
                        leftBounds.fit(intersectPoint);
                        rightBounds.fit(intersectPoint);
                    }
                }
            }

            visitedVertices.push_back(vertex);
        } else {
            // Split plane event
            passedPlane = true;
        }
    }

    return { leftBounds.intersection(inBounds), rightBounds.intersection(inBounds) };
}*/
}
