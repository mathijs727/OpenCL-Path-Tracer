#pragma once
#include "bvh/bvh_nodes.h"
#include "material.h"
#include "vertices.h"

namespace raytracer {
class IMesh {
public:
    virtual const std::vector<VertexSceneData>& getVertices() const = 0;
    virtual const std::vector<TriangleSceneData>& getTriangles() const = 0;
    virtual const std::vector<Material>& getMaterials() const = 0;
    virtual const std::vector<SubBvhNode>& getBvhNodes() const = 0;
    virtual const std::vector<uint32_t>& getEmissiveTriangles() const = 0;

    virtual bool isDynamic() const = 0;
    virtual uint32_t maxNumVertices() const = 0;
    virtual uint32_t maxNumTriangles() const = 0;
    virtual uint32_t maxNumMaterials() const = 0;
    virtual uint32_t maxNumBvhNodes() const = 0;
    virtual void buildBvh() = 0;

    virtual uint32_t getBvhRootNode() const = 0;
};
}
