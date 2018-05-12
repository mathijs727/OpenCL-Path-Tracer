#pragma once
#include "aabb.h"
#include "bvh_nodes.h"
#include "vertices.h"
#include <gsl/gsl>
#include <tuple>
#include <vector>

namespace raytracer {

struct PrimitiveData {
    uint32_t globalIndex;
    AABB bounds;
};

std::vector<PrimitiveData> generatePrimitives(gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);

using PrimInsertIter = std::insert_iterator<std::vector<PrimitiveData>>;
using SplitFunc = std::function<bool(const SubBvhNode& node, gsl::span<const PrimitiveData>, PrimInsertIter left, PrimInsertIter right)>;
std::tuple<uint32_t, std::vector<PrimitiveData>, std::vector<SubBvhNode>> buildBVH(std::vector<PrimitiveData>&& primitives, SplitFunc&& splitFunc);

struct ObjectBin {
    size_t primCount = 0;
    AABB bounds;
};
constexpr size_t BVH_OBJECT_BIN_COUNT = 32;
std::array<ObjectBin, BVH_OBJECT_BIN_COUNT> performObjectBinning(const AABB& nodeBounds, int axis, gsl::span<const PrimitiveData> primitives);

/*

class BVHBuildData {
public:
    BVHBuildData(gsl::span<VertexSceneData> vertices, gsl::span<TriangleSceneData> triangles);

    BVHBuildHelper getBuildHelper();
    BVHAllocator& getAllocator();

private:
    gsl::span<VertexSceneData> vertices;
    gsl::span<TriangleSceneData> triangles;

    std::vector<PrimitiveData> primitives;

    BVHAllocator allocator;
};*/

}
