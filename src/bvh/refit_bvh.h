#pragma once
#include "bvh_nodes.h"
#include "vertices.h"
#include <gsl/gsl>

namespace raytracer {

void refitBVH(gsl::span<SubBVHNode> nodes, uint32_t rootNodeID, gsl::span<const VertexSceneData> vertices, gsl::span<const TriangleSceneData> triangles);

}
