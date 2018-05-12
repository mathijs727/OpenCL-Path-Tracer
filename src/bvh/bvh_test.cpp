#include "bvh_test.h"
#include <array>
#include <iostream>

using namespace raytracer;

void traverse(std::vector<SubBvhNode>& bvhNodes)
{
}

raytracer::BvhTester::BvhTester(std::shared_ptr<Mesh> meshID)
    : m_vertices(meshID->getVertices())
    , m_triangles(meshID->getTriangles())
    , m_bvhNodes(meshID->getBvhNodes())
    , m_rootNode(meshID->getBvhRootNode())
{
}

raytracer::BvhTester::~BvhTester()
{
}

void raytracer::BvhTester::test()
{
    std::cout << "\n\n-----   BVH TEST STARTING   -----\n";
    std::cout << "Number of triangles in model: " << m_triangles.size() << "\n";
    std::cout << "Number of bvh nodes in model: " << m_bvhNodes.size() << "\n";
    std::cout << "Number of bvh nodes visited: " << countNodes(m_rootNode) << " out of " << m_bvhNodes.size() << "\n";
    std::cout << "Recursion depth: " << countDepth(m_rootNode) << "\n\n";

    std::cout << "Number of triangles visited: " << countTriangles(m_rootNode) << " out of " << m_triangles.size() << "\n";
    std::cout << "Average triangle per leaf: " << (float)countTriangles(m_rootNode) / countLeafs(m_rootNode) << "\n";
    std::cout << "Max triangles per leaf: " << maxTrianglesPerLeaf(m_rootNode) << "\n\n";

    // Histogram
    std::cout << "Triangle per leaf histogram:\n";
    const uint32_t max = maxTrianglesPerLeaf(m_rootNode);
    for (int i = 1; i <= 10; i++) {
        float stepLin = (float)max / i;
        float stepQuad = ((stepLin / max) * (stepLin / max)) * max;
        uint32_t stepMax = (uint32_t)stepQuad;
        uint32_t count = countTrianglesLessThen(m_rootNode, stepMax);

        std::cout << "< " << stepMax << ":\t\t" << count << "\n";
    }

    std::cout << "\nTesting node bounds\n";
    testNodeBounds(m_rootNode);
    std::cout << "Success!\n";

    std::cout << "\n\n"
              << std::flush;
}

uint32_t raytracer::BvhTester::countNodes(uint32_t nodeId)
{
    auto& node = m_bvhNodes[nodeId];
    if (node.triangleCount == 0) {
        return countNodes(node.leftChildIndex) + countNodes(node.leftChildIndex + 1) + 1;
    } else {
        return 1;
    }
}

uint32_t raytracer::BvhTester::countDepth(uint32_t nodeId)
{
    auto& node = m_bvhNodes[nodeId];
    if (node.triangleCount == 0) {
        return std::max(countDepth(node.leftChildIndex), countDepth(node.leftChildIndex + 1)) + 1;
    } else {
        return 1;
    }
}

uint32_t raytracer::BvhTester::countTriangles(uint32_t nodeId)
{
    auto& node = m_bvhNodes[nodeId];
    if (node.triangleCount == 0) {
        return countTriangles(node.leftChildIndex) + countTriangles(node.leftChildIndex + 1);
    } else {
        return node.triangleCount;
    }
}

uint32_t raytracer::BvhTester::countLeafs(uint32_t nodeId)
{
    auto& node = m_bvhNodes[nodeId];
    if (node.triangleCount == 0) {
        return countLeafs(node.leftChildIndex) + countLeafs(node.leftChildIndex + 1);
    } else {
        return 1;
    }
}

uint32_t raytracer::BvhTester::maxTrianglesPerLeaf(uint32_t nodeId)
{
    auto& node = m_bvhNodes[nodeId];
    if (node.triangleCount == 0) {
        return std::max(maxTrianglesPerLeaf(node.leftChildIndex), maxTrianglesPerLeaf(node.leftChildIndex + 1));
    } else {
        return node.triangleCount;
    }
}

uint32_t raytracer::BvhTester::countTrianglesLessThen(uint32_t nodeId, uint32_t maxCount)
{
    auto& node = m_bvhNodes[nodeId];
    if (node.triangleCount == 0) {
        return countTrianglesLessThen(node.leftChildIndex, maxCount) + countTrianglesLessThen(node.leftChildIndex + 1, maxCount);
    } else {
        if (node.triangleCount < maxCount)
            return 1;
        else
            return 0;
    }
}

void raytracer::BvhTester::testNodeBounds(uint32_t nodeId)
{
    auto& node = m_bvhNodes[nodeId];
    auto bounds = node.bounds;
    if (node.triangleCount == 0) {
        // Check that both child nodes fit in this node
        assert(node.bounds.contains(m_bvhNodes[node.leftChildIndex + 0].bounds));
        assert(node.bounds.contains(m_bvhNodes[node.leftChildIndex + 1].bounds));
        testNodeBounds(node.leftChildIndex + 0);
        testNodeBounds(node.leftChildIndex + 1);
    } else {
        // Check that all triangles fit in the bounds of this leaf node
        for (const auto& triangle : m_triangles.subspan(node.firstTriangleIndex, node.triangleCount)) {
            auto v1 = m_vertices[triangle.indices[0]].vertex;
            auto v2 = m_vertices[triangle.indices[1]].vertex;
            auto v3 = m_vertices[triangle.indices[2]].vertex;
            assert(bounds.contains(v1));
            assert(bounds.contains(v2));
            assert(bounds.contains(v3));
        }
    }
}
