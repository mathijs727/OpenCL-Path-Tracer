#pragma once
#include"vertices.h"
#include "bvh_nodes.h"
#include "material.h"
#include "types.h"

namespace raytracer
{
class IMesh
{
public:
	virtual const std::vector<VertexSceneData>& getVertices() const = 0;
	virtual const std::vector<TriangleSceneData>& getTriangles() const = 0;
	virtual const std::vector<Material>& getMaterials() const = 0;
	virtual const std::vector<SubBvhNode>& getBvhNodes() const = 0;
	virtual const std::vector<u32>& getEmisiveTriangles() const = 0;

	virtual bool isDynamic() const = 0;
	virtual u32 maxNumVertices() const = 0;
	virtual u32 maxNumTriangles() const = 0;
	virtual u32 maxNumMaterials() const = 0;
	virtual u32 maxNumBvhNodes() const = 0;
	virtual void buildBvh() = 0;

	virtual u32 getBvhRootNode() const = 0;
};
}