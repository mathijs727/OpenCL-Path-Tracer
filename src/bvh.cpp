#include "bvh.h"
#include "scene.h"
#include <stack>
#include <numeric>

using namespace raytracer;

void raytracer::Bvh::build() {
	SceneNode& current_node = _scene.get_root_node();

	std::stack<SceneNode*> node_stack;
	node_stack.push(&current_node);

	while (!node_stack.empty()) {
		SceneNode* current = node_stack.top(); node_stack.pop();
		u32 n = current->meshData.triangle_count;

		ThinBvhNode* nodes = _thinBuffer.data() + _thinBuffer.size();
		_thinBuffer.resize(_thinBuffer.size() + n*2 - 1);

		u32 root = 0;
		nodes[root].triangleFirstIndex = current->meshData.start_triangle_index;
		nodes[root].count = n;
		nodes[root].bounds = create_bounds(nodes[root].triangleFirstIndex, nodes[root].count);
	}
}

void raytracer::Bvh::partition(ThinBvhNode& node, ThinBvhNode* bvhNodeBuffer, u32& nodeCount) {
	u32 leftIndex = nodeCount;
	u32 rightIndex = nodeCount + 1;
	nodeCount += 2;

	auto triangles = _scene._triangle_indices.data() + node.triangleFirstIndex;

	// calculate barycenters
	std::vector<glm::vec3> centres (node.count);
	for (uint i = 0; i < node.count; ++i) {
		auto& triang = triangles[i];
		glm::vec3 vertices[3];
		vertices[0] = (glm::vec3) _scene.GetVertices()[triang.indices[0]].vertex;
		vertices[1] = (glm::vec3) _scene.GetVertices()[triang.indices[1]].vertex;
		vertices[2] = (glm::vec3) _scene.GetVertices()[triang.indices[2]].vertex;
		centres[i] = std::accumulate(std::begin(vertices), std::end(vertices), glm::vec3()) / 3.f;
	}

	// find out which axis to split
	uint split_axis;
	float max_extent = std::numeric_limits<float>::min();
	for (uint i = 0; i < 3; ++i) {
		if (node.bounds.extents[i] > max_extent) {
			max_extent = node.bounds.extents[i];
			split_axis = i;
		}
	}

	// count the amount of triangles in the left node
	uint left_count = 0;
	for (uint i = 0; i < node.count; ++i) {
		if (centres[i][split_axis] < node.bounds.centre[split_axis]) {
			left_count++;
		}
	}

	// decide the separating line
	float separator = node.bounds.centre[split_axis];

	// swap the triangles so the the first left_count triangles are left of the separating line
	uint j = 0;
	for (uint i = 0; i < left_count; ++i) {
		if (centres[i][split_axis] > separator) {
			for (;;++j) {
				if (centres[j][split_axis] < separator) {
					std::swap(centres[i], centres[j]);
					std::swap(triangles[i], triangles[j]);
					break;
				}
			}
		}
	}

	// initialise child nodes
	auto& lNode = bvhNodeBuffer[leftIndex];
	lNode.count = left_count;
	lNode.triangleFirstIndex = node.triangleFirstIndex;
	lNode.bounds = create_bounds(lNode.triangleFirstIndex, lNode.count);

	auto& rNode = bvhNodeBuffer[rightIndex];
	rNode.count = node.count - left_count;
	rNode.triangleFirstIndex = node.triangleFirstIndex + left_count;
	rNode.bounds = create_bounds(rNode.triangleFirstIndex, rNode.count);

	// set this node as not a leaf anymore
	node.count = 0;
	node.leftChildIndex = leftIndex;
}

void  raytracer::Bvh::subdivide(ThinBvhNode& node, ThinBvhNode* bvhNodeBuffer, u32& nodeCount) {
	if (node.count < 3) {
		return;
	}
	partition(node, bvhNodeBuffer, nodeCount);
	subdivide(bvhNodeBuffer[node.leftChildIndex], bvhNodeBuffer, nodeCount);
	subdivide(bvhNodeBuffer[node.leftChildIndex + 1], bvhNodeBuffer, nodeCount);
}

AABB raytracer::Bvh::create_bounds(u32 first_index, u32 count) {
	u32 lastIndex = first_index + count;
	glm::vec3 max(std::numeric_limits<float>::min());
	glm::vec3 min(std::numeric_limits<float>::max());
	for (uint i = first_index; i < lastIndex; ++i) {
		auto& triang = _scene.GetTriangleIndices()[i];
		glm::vec3 vertices[3];
		vertices[0] = (glm::vec3) _scene.GetVertices()[triang.indices[0]].vertex;
		vertices[1] = (glm::vec3) _scene.GetVertices()[triang.indices[1]].vertex;
		vertices[2] = (glm::vec3) _scene.GetVertices()[triang.indices[2]].vertex;
		for (auto& v : vertices) for (int x = 0; i < 3; ++i) {
			max[x] = std::max(max[x], v[x]);
			min[x] = std::min(max[x], v[x]);
		}
	}
	AABB result;
	result.centre = (max + min) / 2.f;
	result.extents = (max - min) / 2.f;
	return result;
}