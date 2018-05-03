#pragma once
#include "aabb.h"
#include "bvh_nodes.h"
#include "types.h"
#include "vertices.h"
#include <glm/glm.hpp>
#include <vector>

namespace raytracer {

class Scene;
struct SceneNode;

class RefittingBvhBuilder {
public:
    RefittingBvhBuilder(){};
    ~RefittingBvhBuilder(){};

    void update(
        const std::vector<VertexSceneData>& vertices,
        const std::vector<TriangleSceneData>& triangles,
        std::vector<SubBvhNode>& outBvhNodes); // BVH may change this
private:
    AABB createBounds(const TriangleSceneData& triangle, const std::vector<VertexSceneData>& vertices);

private:
    // Used during binned BVH construction
    std::vector<AABB> _aabbs;
};
}
