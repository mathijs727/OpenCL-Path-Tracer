#include "bvh_build.h"
#include "aabb.h"
#include "bvh_allocator.h"
#include "bvh_object_split.h"
#include "bvh_spatial_split.h"
#include <algorithm>
#include <array>
#include <functional>
#include <stack>
#include <tuple>
#include <vector>

namespace raytracer {

static constexpr int MIN_PRIMS_PER_LEAF = 3;
static constexpr float SPATIAL_SPLIT_ALPHA = 1e-05f;
static constexpr float SAH_TRAVERSAL_COST = 1.5f;
static constexpr float SAH_INTERSECTION_COST = 1.0f;

static int maxIndex(glm::vec3 vec)
{
    if (vec[0] > vec[1]) {
        if (vec[0] > vec[2])
            return 0;
        else
            return 2;
    } else {
        if (vec[1] > vec[2])
            return 1;
        else
            return 2;
    }
}

static float getSAH(float partialSAH, const AABB& parentBounds)
{
    // Partial SAH: SA(B_1)*|P_1| + SA(B_2)*|P_2|
    return SAH_TRAVERSAL_COST + partialSAH * SAH_INTERSECTION_COST / parentBounds.surfaceArea();
}

static std::vector<PrimitiveData> generatePrimitives(std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles)
{
    std::vector<PrimitiveData> primitives(triangles.size());

    uint32_t i = 0;
    std::transform(triangles.begin(), triangles.end(), primitives.begin(), [&](const TriangleSceneData& triangle) -> PrimitiveData {
        AABB bounds;
        bounds.fit(vertices[(triangle.indices[0])].vertex);
        bounds.fit(vertices[(triangle.indices[1])].vertex);
        bounds.fit(vertices[(triangle.indices[2])].vertex);
        return { i++, bounds };
    });

    return primitives;
}

static AABB computeBounds(std::span<const PrimitiveData> primitives)
{
    AABB bounds;
    for (const auto& primitive : primitives)
        bounds.fit(primitive.bounds);
    return bounds;
}

using SplitFunc = std::function<std::optional<std::pair<AABB, AABB>>(const SubBVHNode& node, std::span<const PrimitiveData>, PrimInsertIter left, PrimInsertIter right)>;
static std::tuple<uint32_t, std::vector<PrimitiveData>, std::vector<SubBVHNode>> buildBVH(std::vector<PrimitiveData>&& startPrimitives, SplitFunc&& splitFunc)
{
    BVHAllocator nodeAllocator;
    std::vector<PrimitiveData> outPrimitives;

    uint32_t rootNodeID = nodeAllocator.allocatePair();
    SubBVHNode& rootNode = nodeAllocator[rootNodeID];
    rootNode.bounds = computeBounds(startPrimitives);
    rootNode.firstTriangleIndex = 0;
    rootNode.triangleCount = static_cast<uint32_t>(startPrimitives.size());

    std::stack<std::pair<uint32_t, std::vector<PrimitiveData>>> stack;
    stack.push({ rootNodeID, std::move(startPrimitives) });

    while (!stack.empty()) {
        // Can't use structured bindings because we want to move the primitives instead of copying them.
        auto [nodeID, primitives] = std::move(stack.top());
        stack.pop();

        std::vector<PrimitiveData> left;
        std::vector<PrimitiveData> right;
        auto optResult = splitFunc(nodeAllocator[nodeID], primitives, std::inserter(left, left.begin()), std::inserter(right, right.begin()));

        if (optResult) {
            uint32_t leftNodeID = nodeAllocator.allocatePair();
            uint32_t rightNodeID = leftNodeID + 1;

            auto& leftNode = nodeAllocator[leftNodeID];
            auto& rightNode = nodeAllocator[rightNodeID];
            std::tie(leftNode.bounds, rightNode.bounds) = *optResult;

            auto& node = nodeAllocator[nodeID]; // After allocation of child nodes because the allocator may move nodes in memory on allocation
            node.leftChildIndex = leftNodeID; // Set child pointer (index) to newly allocated nodes
            node.triangleCount = 0; // Triangle count = 0 -> inner node

            stack.push({ leftNodeID, std::move(left) });
            stack.push({ rightNodeID, std::move(right) });
        } else {
            auto& node = nodeAllocator[nodeID];
            node.firstTriangleIndex = static_cast<uint32_t>(outPrimitives.size());
            node.triangleCount = static_cast<uint32_t>(primitives.size());
            outPrimitives.insert(outPrimitives.end(), primitives.begin(), primitives.end());
        }
    }

    return { rootNodeID, std::move(outPrimitives), std::move(nodeAllocator.getNodesMove()) };
}

using InPlaceSplitFunc = std::function<std::optional<std::tuple<std::span<PrimitiveData>, AABB, std::span<PrimitiveData>, AABB>>(const SubBVHNode& node, std::span<PrimitiveData>)>;
static std::tuple<uint32_t, std::vector<PrimitiveData>, std::vector<SubBVHNode>> buildBVHInPlace(std::vector<PrimitiveData>&& startPrimitives, InPlaceSplitFunc&& splitFunc)
{
    BVHAllocator nodeAllocator;

    uint32_t rootNodeID = nodeAllocator.allocatePair();
    SubBVHNode& rootNode = nodeAllocator[rootNodeID];
    rootNode.bounds = computeBounds(startPrimitives);
    rootNode.firstTriangleIndex = 0;
    rootNode.triangleCount = static_cast<uint32_t>(startPrimitives.size());

    struct StackItem {
        uint32_t nodeID;
        uint32_t primOffset;
        std::span<PrimitiveData> primitives;
    };
    std::stack<StackItem> stack;
    stack.push(StackItem { rootNodeID, 0, startPrimitives });

    while (!stack.empty()) {
        // Can't use structured bindings because we want to move the primitives instead of copying them.
        auto [nodeID, primOffset, primitives] = std::move(stack.top());
        stack.pop();

        auto optResult = splitFunc(nodeAllocator[nodeID], primitives);

        if (optResult) {
            uint32_t leftNodeID = nodeAllocator.allocatePair();
            uint32_t rightNodeID = leftNodeID + 1;

            std::span<PrimitiveData> leftPrimitives;
            std::span<PrimitiveData> rightPrimitives;
            auto& leftNode = nodeAllocator[leftNodeID];
            auto& rightNode = nodeAllocator[rightNodeID];
            std::tie(leftPrimitives, leftNode.bounds, rightPrimitives, rightNode.bounds) = *optResult;

            auto& node = nodeAllocator[nodeID]; // After allocation of child nodes because the allocator may move nodes in memory on allocation
            node.leftChildIndex = leftNodeID; // Set child pointer (index) to newly allocated nodes
            node.triangleCount = 0; // Triangle count = 0 -> inner node

            stack.push({ leftNodeID, primOffset, leftPrimitives });
            stack.push({ rightNodeID, primOffset + (uint32_t)leftPrimitives.size(), rightPrimitives });
        } else {
            auto& node = nodeAllocator[nodeID];
            node.firstTriangleIndex = primOffset;
            node.triangleCount = static_cast<uint32_t>(primitives.size());
        }
    }

    return { rootNodeID, std::move(startPrimitives), std::move(nodeAllocator.getNodesMove()) };
}

BvhBuildReturnType buildBinnedBVH(std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles)
{
    auto primitives = generatePrimitives(vertices, triangles); // Create primitive refences
    OriginalPrimitives originalPrimitives { vertices, triangles };

    auto [rootNodeID, reorderedPrimitives, bvhNodes] = buildBVHInPlace(std::move(primitives), [&](const SubBVHNode& node, std::span<PrimitiveData> primitives) -> std::optional<std::tuple<std::span<PrimitiveData>, AABB, std::span<PrimitiveData>, AABB>> {
        if (primitives.size() <= MIN_PRIMS_PER_LEAF)
            return {};

        float thresholdSAH = primitives.size() * SAH_INTERSECTION_COST;
        if (auto split = findObjectSplitBinned(node.bounds, primitives, originalPrimitives, std::array { 1, 2, 3 }); split && getSAH(split->partialSAH, node.bounds) < thresholdSAH) {
            size_t splitIndex = performObjectSplitInPlace(primitives, *split);
            auto leftPrims = primitives.subspan(0, splitIndex);
            auto rightPrims = primitives.subspan(splitIndex, primitives.size() - splitIndex);
            assert(leftPrims.size() > 0 && rightPrims.size() > 0);
            return { { leftPrims, split->leftBounds, rightPrims, split->rightBounds } };
        } else {
            return {};
        }
    });

    // Convert primitive references to triangles
    std::vector<TriangleSceneData> outTriangles(reorderedPrimitives.size());
    std::transform(reorderedPrimitives.begin(), reorderedPrimitives.end(), outTriangles.begin(), [&](const PrimitiveData& primitive) {
        return triangles[primitive.globalIndex];
    });
    return { rootNodeID, outTriangles, bvhNodes };
}

BvhBuildReturnType buildBinnedFastBVH(std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles)
{
    auto primitives = generatePrimitives(vertices, triangles); // Create primitive refences
    OriginalPrimitives originalPrimitives { vertices, triangles };

    auto [rootNodeID, reorderedPrimitives, bvhNodes] = buildBVHInPlace(std::move(primitives), [&](const SubBVHNode& node, std::span<PrimitiveData> primitives) -> std::optional<std::tuple<std::span<PrimitiveData>, AABB, std::span<PrimitiveData>, AABB>> {
        if (primitives.size() <= MIN_PRIMS_PER_LEAF)
            return {};

        float thresholdSAH = primitives.size() * SAH_INTERSECTION_COST;
        int axis = maxIndex(node.bounds.extent());
        if (auto split = findObjectSplitBinned(node.bounds, primitives, originalPrimitives, std::array { axis }); split && getSAH(split->partialSAH, node.bounds) < thresholdSAH) {
            size_t splitIndex = performObjectSplitInPlace(primitives, *split);
            auto leftPrims = primitives.subspan(0, splitIndex);
            auto rightPrims = primitives.subspan(splitIndex, primitives.size() - splitIndex);
            return { { leftPrims, split->leftBounds, rightPrims, split->rightBounds } };
        } else {
            return {};
        }
    });

    // Convert primitive references to triangles
    std::vector<TriangleSceneData> outTriangles(reorderedPrimitives.size());
    std::transform(reorderedPrimitives.begin(), reorderedPrimitives.end(), outTriangles.begin(), [&](const PrimitiveData& primitive) {
        return triangles[primitive.globalIndex];
    });
    return { rootNodeID, outTriangles, bvhNodes };
}

BvhBuildReturnType buildSpatialSplitBVH(std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles)
{
    auto startPrimitives = generatePrimitives(vertices, triangles); // Create primitive refences
    OriginalPrimitives originalPrimitives { vertices, triangles };

    auto rootNodeSurfaceArea = computeBounds(startPrimitives).surfaceArea();

    auto [rootNodeID, reorderedPrimitives, bvhNodes] = buildBVH(std::move(startPrimitives), [&](const SubBVHNode& node, std::span<const PrimitiveData> primitives, PrimInsertIter left, PrimInsertIter right) -> std::optional<std::pair<AABB, AABB>> {
        if (primitives.size() <= MIN_PRIMS_PER_LEAF)
            return {};

        float thresholdSAH = primitives.size() * SAH_INTERSECTION_COST;
        if (auto objectSplitOpt = findObjectSplitBinned(node.bounds, primitives, originalPrimitives, std::array { 0, 1, 2 }); objectSplitOpt && getSAH(objectSplitOpt->partialSAH, node.bounds) < thresholdSAH) {
            float alpha = objectSplitOpt->leftBounds.intersection(objectSplitOpt->rightBounds).surfaceArea() / rootNodeSurfaceArea;
            if (alpha > SPATIAL_SPLIT_ALPHA) {
                if (auto spatialSplitOpt = findSpatialSplitBinned(node.bounds, primitives, originalPrimitives, std::array { 0, 1, 2 }); spatialSplitOpt && getSAH(spatialSplitOpt->partialSAH, node.bounds) < thresholdSAH) {
                    return performSpatialSplit(primitives, originalPrimitives, *spatialSplitOpt, left, right);
                }
            }
            return performObjectSplit(primitives, originalPrimitives, *objectSplitOpt, left, right);
        } else if (auto spatialSplitOpt = findSpatialSplitBinned(node.bounds, primitives, originalPrimitives, std::array { 0, 1, 2 }); spatialSplitOpt && getSAH(spatialSplitOpt->partialSAH, node.bounds) < thresholdSAH) {
            return performSpatialSplit(primitives, originalPrimitives, *spatialSplitOpt, left, right);
        } else {
            return {};
        }
    });

    // Convert primitive references to triangles
    std::vector<TriangleSceneData> outTriangles(reorderedPrimitives.size());
    std::transform(reorderedPrimitives.begin(), reorderedPrimitives.end(), outTriangles.begin(), [&](const PrimitiveData& primitive) {
        return triangles[primitive.globalIndex];
    });
    return { rootNodeID, outTriangles, bvhNodes };
}
}
