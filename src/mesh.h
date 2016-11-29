#pragma once

#include "glm.hpp"
#include "types.h"
#include <vector>
#include "shapes.h"

struct TriangleIndexes
{
	u32 first;
	u32 second;
	u32 third;
};

struct Vertex
{
	glm::vec3 point;
};

class aiMesh;

namespace raytracer {
class Mesh
{
public:
	std::vector<Vertex> _vertices;
	std::vector<TriangleIndexes> _triangleIndices;

	Triangle getTriangle(const TriangleIndexes& indexes) const {
		Triangle triangle;
		triangle.points[0] = _vertices[indexes.first].point;
		triangle.points[1] = _vertices[indexes.second].point;
		triangle.points[2] = _vertices[indexes.third].point;
		return triangle;
	}

	bool isValid() const { return !_triangleIndices.empty() && !_vertices.empty(); }
	void addData(aiMesh* mesh);
	static Mesh LoadFromFile(const char* file);
};
}




