#pragma once

#include "glm.hpp"
#include "types.h"
#include <vector>
#include "shapes.h"
#include "transform.h"

struct aiMesh;

namespace raytracer {

struct MeshFace
{
	glm::u32vec3 indices;
	glm::vec3 normal;
};

class Mesh
{
public:
	std::vector<glm::vec4> _vertices; // extra value for cl alignment
	std::vector<MeshFace> _faces;

	bool isValid() const { return !_faces.empty() && !_vertices.empty(); }
	void addData(aiMesh* mesh, const glm::mat4&);
	static Mesh LoadFromFile(const char* file, const Transform& offset = Transform());
};
}




