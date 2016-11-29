#pragma once

#include "glm.hpp"
#include "types.h"
#include <vector>
#include "shapes.h"

class aiMesh;

namespace raytracer {
class Mesh
{
public:
	std::vector<glm::vec3> _vertices;
	std::vector<glm::u32vec3> _triangleIndices;

	bool isValid() const { return !_triangleIndices.empty() && !_vertices.empty(); }
	void addData(aiMesh* mesh);
	static Mesh LoadFromFile(const char* file);
};
}




