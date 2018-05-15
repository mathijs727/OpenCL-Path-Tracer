#pragma once
#include "bvh/bvh_nodes.h"
#include "material.h"
#include "vertices.h"
#include <gsl/gsl>

namespace raytracer {
class IMesh {
public:
    virtual gsl::span<const VertexSceneData> getVertices() const = 0;
    virtual gsl::span<const TriangleSceneData> getTriangles() const = 0;
    virtual gsl::span<const Material> getMaterials() const = 0;
    virtual gsl::span<const SubBvhNode> getBvhNodes() const = 0;
    virtual gsl::span<const uint32_t> getEmissiveTriangles() const = 0;

    virtual AABB getBounds() const = 0;
    virtual bool isDynamic() const = 0;
    virtual uint32_t maxNumVertices() const = 0;
    virtual uint32_t maxNumTriangles() const = 0;
    virtual uint32_t maxNumMaterials() const = 0;
    virtual uint32_t maxNumBvhNodes() const = 0;
    virtual void buildBvh() = 0;

    virtual uint32_t getBvhRootNode() const = 0;
};
}
