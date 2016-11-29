#pragma once

#include "glm.hpp"
#include "types.h"
#include <vector>
#include "shapes.h"
#include "transform.h"

struct aiMesh;

namespace raytracer {
class Mesh
{
public:
	std::vector<glm::vec4> _vertices; // extra value for cl alignment
	std::vector<glm::u32vec3> _triangleIndices;

	bool isValid() const { return !_triangleIndices.empty() && !_vertices.empty(); }
	void addData(aiMesh* mesh, const glm::mat4&);
	static Mesh LoadFromFile(const char* file, const Transform& offset = Transform());
};
}




