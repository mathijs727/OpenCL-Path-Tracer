#pragma once
#include "glm.hpp"
#include "types.h"
#include <vector>
#include "shapes.h"
#include "transform.h"
#include "material.h"
#include "linear_allocator.h"
#include "binned_bvh.h"
#include "vertices.h"

struct aiMesh;
struct aiScene;

namespace raytracer {

class Mesh
{
public:
	Mesh() {}
	~Mesh() { }
	
	void loadFromFile(const char* file,	const Transform& offset = Transform());

	const std::vector<TriangleSceneData>& getTriangles() const { return _triangles; }
	const std::vector<VertexSceneData>& getVertices() const { return _vertices; }
	const std::vector<Material>& getMaterials() const { return _materials; }
	const std::vector<SubBvhNode>& getBvhNodes() const { return _bvh_nodes; }
	
	u32 getRootBvhNode() const { return _root_bvh_node; };
private:
	void addSubMesh(const aiScene* scene, uint mesh_index, const glm::mat4& transform_matrix);
private:
	std::vector<TriangleSceneData> _triangles;
	std::vector<VertexSceneData> _vertices;
	std::vector<Material> _materials;
	std::vector<SubBvhNode> _bvh_nodes;

	u32 _root_bvh_node;
};
}

