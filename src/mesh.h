#pragma once

#include "glm.hpp"
#include "types.h"
#include <vector>
#include "shapes.h"
#include "transform.h"
#include "material.h"

struct aiMesh;
struct aiScene;

namespace raytracer {

class Mesh
{
public:
	Material _material;
	std::vector<glm::vec4> _vertices; // extra value for cl alignment
	std::vector<glm::vec4> _normals; // extra value for cl alignment
	std::vector<glm::u32vec3> _faces;
	std::vector<glm::vec2> _textureCoords;
	bool isValid() const { return !_faces.empty() && !_vertices.empty(); }
	static Mesh makeMesh(const aiScene* scene, uint mesh_index, const glm::mat4& transform_matrix);
	static std::vector<Mesh> LoadFromFile(const char* file, const Transform& offset = Transform());
};
}




