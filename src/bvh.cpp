#include "bvh.h"
#include "scene.h"
#include <stack>

using namespace raytracer;

void subdivide(ThinBvhNode& node, ThinBvhNode* buffer, u32& nodeCount) {
	if (node.count < 3) {
		return;
	}
	node.leftChildIndex = nodeCount;
	nodeCount += 2;

	subdivide(buffer[node.leftChildIndex], buffer, nodeCount);
	subdivide(buffer[node.leftChildIndex + 1], buffer, nodeCount);
}

AABB create_bounds(const Scene& scene, u32 first_index, u32 count) {

}

void raytracer::Bvh::build(Scene& scene) {
	SceneNode& current_node = scene.get_root_node();

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
		nodes[root].bounds = create_bounds(scene, nodes[root].triangleFirstIndex, nodes[root].count);

	}
}
