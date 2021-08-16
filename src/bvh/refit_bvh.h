#pragma once
#include "bvh_nodes.h"
#include "vertices.h"
#include <span>

namespace raytracer {

void refitBVH(std::span<SubBVHNode> nodes, uint32_t rootNodeID, std::span<const VertexSceneData> vertices, std::span<const TriangleSceneData> triangles);

}
