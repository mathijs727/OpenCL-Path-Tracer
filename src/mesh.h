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

	/*const Material& getMaterial() const { return _material; };
	const std::vector<glm::vec4>& getVertices() const { return _vertices; };
	const std::vector<glm::vec4>& getNormals() const { return _normals; };
	const std::vector<glm::u32vec3>& getFaces() const { return _faces; };
	const std::vector<glm::vec2>& getTextureCoords() const { return _textureCoords; };*/
private:
	/*Material _material;
	std::vector<glm::vec4> _vertices; // extra value for cl alignment
	std::vector<glm::vec4> _normals; // extra value for cl alignment
	std::vector<glm::u32vec3> _faces;
	std::vector<glm::vec2> _textureCoords;*/

	u32 _firstTriangleIndex;
	u32 _triangleCount;
	u32 _bvhRootNode;
};
}

