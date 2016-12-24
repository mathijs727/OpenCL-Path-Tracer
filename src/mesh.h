#pragma once
#include "glm.hpp"
#include "types.h"
#include <vector>
#include "shapes.h"
#include "transform.h"
#include "material.h"
#include "binned_bvh.h"
#include "vertices.h"
#include "imesh.h"

struct aiMesh;
struct aiScene;

namespace raytracer {

class Mesh : public IMesh
{
public:
	Mesh() {}
	~Mesh() { }
	
	void loadFromFile(const char* file,	const Transform& offset = Transform());

	const std::vector<VertexSceneData>& getVertices() const override { return _vertices; }
	const std::vector<TriangleSceneData>& getTriangles() const override { return _triangles; }
	const std::vector<Material>& getMaterials() const override { return _materials; }
	const std::vector<SubBvhNode>& getBvhNodes() const override { return _bvh_nodes; }
	
	u32 getBvhRootNode() const override { return _bvh_root_node; };

	bool isDynamic() const override { return false; };
	void buildBvh() override { };// Only necessary for dynamic objects
private:
	void addSubMesh(const aiScene* scene, uint mesh_index, const glm::mat4& transform_matrix);
private:
	std::vector<TriangleSceneData> _triangles;
	std::vector<VertexSceneData> _vertices;
	std::vector<Material> _materials;
	std::vector<SubBvhNode> _bvh_nodes;

	u32 _bvh_root_node;
};
}

