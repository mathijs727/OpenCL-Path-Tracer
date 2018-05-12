#include "bvh_build.h"
#include <algorithm>
#include <stack>
#include <vector>

namespace raytracer {

class BVHAllocator {
public:
    uint32_t allocatePair();

    const SubBvhNode& operator[](uint32_t i) const;
    SubBvhNode& operator[](uint32_t i);

    std::vector<SubBvhNode>&& getNodesMove();

private:
    std::vector<SubBvhNode> m_nodes;
};

std::vector<PrimitiveData> generatePrimitives(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles)
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

static AABB computeBounds(gsl::span<const PrimitiveData> primitives)
{
    AABB bounds;
    for (const auto& primitive : primitives)
        bounds.fit(primitive.bounds);
    return bounds;
}

std::tuple<uint32_t, std::vector<PrimitiveData>, std::vector<SubBvhNode>> buildBVH(std::vector<PrimitiveData>&& startPrimitives, SplitFunc&& splitFunc)
{
    BVHAllocator nodeAllocator;
    std::vector<PrimitiveData> outPrimitives;

    uint32_t rootNodeID = nodeAllocator.allocatePair();
    SubBvhNode& rootNode = nodeAllocator[rootNodeID];
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

std::array<ObjectBin, BVH_OBJECT_BIN_COUNT> performObjectBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives)
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

uint32_t BVHAllocator::allocatePair()
{
    uint32_t leftIndex = (uint32_t)m_nodes.size();
    m_nodes.emplace_back();
    m_nodes.emplace_back();
    return leftIndex;
}

const SubBvhNode& BVHAllocator::operator[](uint32_t i) const
{
    return m_nodes[i];
}

SubBvhNode& BVHAllocator::operator[](uint32_t i)
{
    return m_nodes[i];
}

std::vector<SubBvhNode>&& BVHAllocator::getNodesMove()
{
    return std::move(m_nodes);
}

}
