#include "refit_bvh.h"
#include "aabb.h"

namespace raytracer {

AABB recurse(std::span<SubBVHNode> nodes, uint32_t nodeID, std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles)
{
    auto& node = nodes[nodeID];
    if (node.triangleCount == 0) { // Inner node
        AABB newBounds;
        newBounds.fit(recurse(nodes, node.leftChildIndex + 0, vertices, triangles));
        newBounds.fit(recurse(nodes, node.leftChildIndex + 1, vertices, triangles));
        node.bounds = newBounds;
        return newBounds;
    } else { // Leaf node
        AABB newBounds;
        for (uint32_t i = node.firstTriangleIndex; i < node.firstTriangleIndex + node.triangleCount; i++) {
            const auto& triangle = triangles[i];
            glm::vec3 v1 = vertices[triangle.indices[0]].vertex;
            glm::vec3 v2 = vertices[triangle.indices[1]].vertex;
            glm::vec3 v3 = vertices[triangle.indices[2]].vertex;
            newBounds.fit(v1);
            newBounds.fit(v2);
            newBounds.fit(v3);
        }
        node.bounds = newBounds;
        return newBounds;
    }
}

void refitBVH(std::span<SubBVHNode> nodes, uint32_t rootNodeID, std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles)
{
    recurse(nodes, rootNodeID, vertices, triangles);
}
}
