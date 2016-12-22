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
	Mesh() : _firstTriangleIndex(-1), _triangleCount(0), _bvhRootNode(-1) {}
	~Mesh() { }
	
	bool isValid() const { return _firstTriangleIndex >= 0 && _triangleCount > 0 && _bvhRootNode >= 0; };

	void loadFromFile(const char* file,	const Transform& offset = Transform());

	u32 getBvhRoot() const { return _bvhRootNode; };
	u32 getFirstTriangleIndex() const { return _firstTriangleIndex; };
	u32 getTriangleCount() const { return _triangleCount; };
private:
	void addSubMesh(const aiScene* scene, uint mesh_index, const glm::mat4& transform_matrix);
private:
	u32 _firstTriangleIndex;
	u32 _triangleCount;
	u32 _bvhRootNode;
};
}

