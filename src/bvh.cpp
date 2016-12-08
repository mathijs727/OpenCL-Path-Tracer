#include "bvh.h"
#include "scene.h"
#include <stack>
#include <numeric>
#include <array>
#include <algorithm>

using namespace raytracer;

void raytracer::Bvh::build() {
	SceneNode& current_node = _scene.get_root_node();

	std::stack<SceneNode*> node_stack;
	node_stack.push(&current_node);

	while (!node_stack.empty()) {
		SceneNode* current = node_stack.top(); node_stack.pop();
		// Visit children
		for (int i = 0; i < current->children.size(); i++)
		{
			node_stack.push(current->children[i].get());
		}
		u32 n = current->meshData.triangle_count;
		if (n < 1)
			continue;

		_thinBuffer.reserve(_thinBuffer.size() + n*2);

		u32 rootIndex = allocate();
		auto& root = _thinBuffer[rootIndex];
		root.firstTriangleIndex = current->meshData.start_triangle_index;
		root.triangleCount = n;
		root.bounds = create_bounds(root.firstTriangleIndex, root.triangleCount);

		// Add a dummy node so that every pair of children nodes is cache line aligned.
		allocate();

		subdivide(root);
	}
}

u32 raytracer::Bvh::allocate()
{
	_thinBuffer.emplace_back();
	return _poolPtr++;
}

void  raytracer::Bvh::subdivide(ThinBvhNode& node) {
	if (node.triangleCount < 4)
		return;

	u32 leftChildIndex = allocate();
	u32 rightChildIndex = allocate();
	partition(node, leftChildIndex);// Divides our triangles over our children and calculates their bounds
	subdivide(_thinBuffer[leftChildIndex]);
	subdivide(_thinBuffer[rightChildIndex]);
}

void raytracer::Bvh::partition(ThinBvhNode& node, u32 leftIndex) {
	u32 rightIndex = leftIndex + 1;

	auto* triangles = _scene._triangle_indices.data() + node.firstTriangleIndex;

	// calculate barycenters
	std::vector<glm::vec3> centres(node.triangleCount);
	for (uint i = 0; i < node.triangleCount; ++i) {
		auto& triang = triangles[i];
		std::array<glm::vec3, 3> vertices;
		vertices[0] = (glm::vec3) _scene.GetVertices()[triang.indices[0]].vertex;
		vertices[1] = (glm::vec3) _scene.GetVertices()[triang.indices[1]].vertex;
		vertices[2] = (glm::vec3) _scene.GetVertices()[triang.indices[2]].vertex;
		centres[i] = std::accumulate(std::begin(vertices), std::end(vertices), glm::vec3()) / 3.f;
	}

	// TODO: better and more efficient splitting than this
	// find out which axis to split 
	uint split_axis;
	float max_extent = std::numeric_limits<float>::min();
	for (uint i = 0; i < 3; ++i) {
		if (node.bounds.extents[i] > max_extent) {
			max_extent = node.bounds.extents[i];
			split_axis = i;
		}
	}

	for (u32 i = 0; i < node.triangleCount; i++)
	{
		for (u32 j = i; j < node.triangleCount; j++)
		{
			if (centres[i][split_axis] < centres[j][split_axis])
			{
				std::swap(centres[i], centres[j]);
				std::swap(triangles[i], triangles[j]);
			}
		}
	}
	u32 left_count = node.triangleCount / 2;
	/*// decide the separating line
	float separator = 0.0f;
	for (uint i = 0; i < node.triangleCount; ++i)
	{
		u32 left = 0;
		for (uint j = 0; j < node.triangleCount; ++j)
		{
			if (centres[i][split_axis] <= centres[j][split_axis])
				left++;
		}

		if (left == node.triangleCount / 2)
		{
			separator = centres[i][split_axis];
			break;
		}
	}

	// count the amount of triangles in the left node
	uint left_count = node.triangleCount / 2;

	// swap the triangles so the the first left_count triangles are left of the separating line
	uint j = left_count;
	for (uint i = 0; i < left_count; ++i) {
		if (centres[i][split_axis] > separator) {
			for (;;++j) {
				if (centres[j][split_axis] <= separator) {
					std::swap(centres[i], centres[j]);
					std::swap(triangles[i], triangles[j]);
					break;
				}
			}
		}
	}*/

	// initialize child nodes
	auto& lNode = _thinBuffer[leftIndex];
	lNode.triangleCount = left_count;
	lNode.firstTriangleIndex = node.firstTriangleIndex;
	lNode.bounds = create_bounds(lNode.firstTriangleIndex, lNode.triangleCount);

	auto& rNode = _thinBuffer[rightIndex];
	rNode.triangleCount = node.triangleCount - left_count;
	rNode.firstTriangleIndex = node.firstTriangleIndex + left_count;
	rNode.bounds = create_bounds(rNode.firstTriangleIndex, rNode.triangleCount);

	// set this node as not a leaf anymore
	node.triangleCount = 0;
	node.leftChildIndex = leftIndex;
}

AABB raytracer::Bvh::create_bounds(u32 first_index, u32 count) {
	if (count == 0)
		_asm nop;

	u32 lastIndex = first_index + count;
	glm::vec3 max(std::numeric_limits<float>::min());
	glm::vec3 min(std::numeric_limits<float>::max());
	for (uint i = first_index; i < lastIndex; ++i) {
		auto& triang = _scene.GetTriangleIndices()[i];
		glm::vec3 vertices[3];
		vertices[0] = (glm::vec3) _scene.GetVertices()[triang.indices[0]].vertex;
		vertices[1] = (glm::vec3) _scene.GetVertices()[triang.indices[1]].vertex;
		vertices[2] = (glm::vec3) _scene.GetVertices()[triang.indices[2]].vertex;
		for (auto& v : vertices)
		{
			max = glm::max(max, v);
			min = glm::min(min, v);
		}
	}
	AABB result;
	result.centre = (max + min) / 2.f;
	result.extents = (max - min) / 2.f;
	return result;
}