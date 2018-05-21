#pragma once
#include "aabb.h"
#include "types.h"
#include "vertices.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace raytracer {
struct SpatialSplitRef {
    uint32_t triangleIndex;
    AABB clippedBounds;
    bool operator==(const SpatialSplitRef& other) const { return triangleIndex == other.triangleIndex; }
};
}

namespace std {
template <>
struct hash<raytracer::SpatialSplitRef> {
    size_t operator()(const raytracer::SpatialSplitRef& x) const { return hash<int>()(x.triangleIndex); }
};
}

namespace raytracer {

class Scene;
struct SceneNode;
struct SubBVHNode;

struct ObjectBin {
    uint32_t triangleCount;
    AABB bounds;
    ObjectBin(uint32_t triangleCount = 0, AABB bounds = AABB())
        : triangleCount(triangleCount)
        , bounds(bounds)
    {
    }
};

struct SpatialSplitBin {
    AABB bounds;
    std::vector<SpatialSplitRef> refs;
};

struct FinalSplit {
    AABB bounds;
    std::unordered_map<uint32_t, AABB> trianglesAABB;
};

class SbvhBuilder {

public:
    SbvhBuilder(){};
    ~SbvhBuilder(){};

    uint32_t build(
        std::vector<VertexSceneData>& vertices,
        std::vector<TriangleSceneData>& triangles,
        std::vector<SubBVHNode>& outBvhNodes); // BVH may change this
private:
    void subdivide(uint32_t nodeId, uint32_t level);
    bool partition(uint32_t nodeId);
    AABB createBounds(uint32_t triangleIndex);
    uint32_t allocateNodePair();
    void makeObjectBins(uint32_t nodeId, uint32_t axis, ObjectBin* bins);
    void makeSpatialBins(uint32_t nodeId, uint32_t axis, SpatialSplitBin* bins);
    bool doSingleObjectSplit(uint32_t nodeId, uint32_t axis, uint32_t split, float& sah, ObjectBin* bins);
    bool doSingleSpatialSplit(uint32_t nodeId, uint32_t axis, uint32_t split, float& sah, FinalSplit& outLeft, FinalSplit& outRight, SpatialSplitBin* bins);
    raytracer::AABB clipTriangleBounds(AABB bounds, uint32_t triangleId);
    bool checkNode(uint32_t nodeId);

public:
    uint32_t _totalSplits;
    uint32_t _spatialSplits;
    uint32_t _totalNodes;

private:
    // Used during binned BVH construction
    std::vector<TriangleSceneData>* _triangles;
    std::vector<VertexSceneData>* _vertices;
    std::vector<SubBVHNode>* _bvh_nodes;
    std::vector<std::unordered_map<uint32_t, AABB>> _node_triangle_list;
    std::vector<glm::vec3> _centres;
    std::vector<AABB> _aabbs;
};
}
