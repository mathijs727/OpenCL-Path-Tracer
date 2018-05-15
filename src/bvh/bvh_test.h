#pragma once
#include "bvh_nodes.h"
#include "model/mesh.h"
#include "types.h"
#include "vertices.h"
#include <gsl/gsl>

namespace raytracer {
class BvhTester {
public:
    BvhTester(std::shared_ptr<Mesh> meshPtr);
    ~BvhTester();

    void test();

private:
    uint32_t countNodes(uint32_t nodeId);
    uint32_t countDepth(uint32_t nodeId);
    uint32_t countTriangles(uint32_t nodeId);
    uint32_t countLeafs(uint32_t nodeId);
    uint32_t maxTrianglesPerLeaf(uint32_t nodeId);
    uint32_t countTrianglesLessThen(uint32_t nodeId, uint32_t maxCount);

    bool testNodeBounds(uint32_t nodeId);

private:
    uint32_t m_rootNode;
    gsl::span<const VertexSceneData> m_vertices;
    gsl::span<const TriangleSceneData> m_triangles;
    gsl::span<const SubBvhNode> m_bvhNodes;
};
}
