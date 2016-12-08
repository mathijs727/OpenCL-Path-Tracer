#include "bvh.h"
#include "scene.h"
#include <stack>

using namespace raytracer;

void raytracer::Bvh::build() {
	SceneNode& current_node = _scene.get_root_node();

	std::stack<SceneNode&> node_stack;
	node_stack.push(current_node);

	while (!node_stack.empty()) {
		SceneNode& current = node_stack.top(); node_stack.pop();
		u32 n = current.meshData.triangle_count;

		ThinBvhNode* nodes = _thinBuffer.data() + _thinBuffer.size();
		_thinBuffer.resize(_thinBuffer.size() + n*2 - 1);

		u32 root = 0;
		nodes[root].triangleFirstIndex = current.meshData.start_triangle_index;
		nodes[root].count = n;
		nodes[root].bounds = create_bounds(nodes[root].triangleFirstIndex, nodes[root].count);
	}
}

void  raytracer::Bvh::partition(ThinBvhNode& node, ThinBvhNode* buffer, u32& nodeCount) {
	u32 leftIndex = nodeCount;
	u32 rightIndex = nodeCount + 1;
	nodeCount += 2;

	u32 lastIndex = node.triangleFirstIndex + node.count;

	for (int i = node.triangleFirstIndex; i < lastIndex; ++i) {
		auto& triang = _scene.GetTriangleIndices()[i];
		
	}

	node.count = 0;
	node.leftChildIndex = leftIndex;
}

void  raytracer::Bvh::subdivide(ThinBvhNode& node, ThinBvhNode* buffer, u32& nodeCount) {
	if (node.count < 3) {
		return;
	}
	partition(node, buffer, nodeCount);
	subdivide(buffer[node.leftChildIndex], buffer, nodeCount);
	subdivide(buffer[node.leftChildIndex + 1], buffer, nodeCount);
}

AABB raytracer::Bvh::create_bounds(u32 first_index, u32 count) {
	u32 lastIndex = first_index + count;
	glm::vec3 max(std::numeric_limits<float>::min());
	glm::vec3 min(std::numeric_limits<float>::max());
	for (int i = first_index; i < lastIndex; ++i) {
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