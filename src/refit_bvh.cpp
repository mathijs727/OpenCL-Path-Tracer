#include "refit_bvh.h"
#include <array>
#include <iostream>

using namespace raytracer;

void raytracer::RefittingBvhBuilder::update(
	const std::vector<VertexSceneData>& vertices,
	const std::vector<TriangleSceneData>& triangles,
	std::vector<SubBvhNode>& outBvhNodes)
{
	// Calculate AABBs
	for (auto& triangle : triangles) {
		_aabbs.push_back(createBounds(triangle, vertices));
	}

	// Assumes all leaf nodes are at the highest indices and that a parent always has a lower index
	// than its children. If this is not the case than this code should be replaced by a (slower)
	// recursive algorithm.
	for (int i = (int)outBvhNodes.size() - 1; i >= 0; i--)
	{
		SubBvhNode& node = outBvhNodes[i];
		if (node.triangleCount == 0)
		{
			// Parent node
			AABB newBounds;
			newBounds.fit(outBvhNodes[node.firstTriangleIndex + 0].bounds);
			newBounds.fit(outBvhNodes[node.firstTriangleIndex + 1].bounds);
			node.bounds = newBounds;
		}
		else {
			// Leaf node
			AABB newBounds;
			u32 end = node.firstTriangleIndex + node.triangleCount;
			for (u32 j = node.firstTriangleIndex; j < end; j++)
			{
				newBounds.fit(_aabbs[j]);
			}
			node.bounds = newBounds;
		}
	}
}

AABB raytracer::RefittingBvhBuilder::createBounds(
	const TriangleSceneData& triangle,
	const std::vector<VertexSceneData>& vertices)
{
	glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

	std::array<glm::vec3, 3> face;
	face[0] = (glm::vec3) vertices[triangle.indices[0]].vertex;
	face[1] = (glm::vec3) vertices[triangle.indices[1]].vertex;
	face[2] = (glm::vec3) vertices[triangle.indices[2]].vertex;
	for (auto& v : face)
	{
		min = glm::min(min, v);
		max = glm::max(max, v);
	}

	AABB result;
	result.min = min;
	result.max = max;
	return result;
}
