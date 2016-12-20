#include "bvh.h"
#include "scene.h"
#include <stack>
#include <numeric>
#include <array>
#include <algorithm>

using namespace raytracer;

void raytracer::Bvh::buildThinBvhs() {
	_thinBuffer.clear();
	_thinPoolPtr = 0;

	SceneNode& current_node = _scene.get_root_node();

	std::stack<SceneNode*> node_stack;
	node_stack.push(&current_node);
	while (!node_stack.empty()) {
		SceneNode* current = node_stack.top(); node_stack.pop();
		// Visit children
		for (uint i = 0; i < current->children.size(); i++)
		{
			node_stack.push(current->children[i].get());
		}
		u32 n = current->meshData.triangle_count;
		if (n < 1)
			continue;

		_thinBuffer.reserve(_thinBuffer.size() + n*2);

		current->meshData.thinBvhIndex = _thinPoolPtr;

		u32 rootIndex = allocateThinNode();
		auto& root = _thinBuffer[rootIndex];
		root.firstTriangleIndex = current->meshData.start_triangle_index;
		root.triangleCount = n;
		root.bounds = create_bounds(root.firstTriangleIndex, root.triangleCount);

		// Add a dummy node so that every pair of children nodes is cache line aligned.
		allocateThinNode();

		subdivide(root);
	}
}

void raytracer::Bvh::updateTopLevelBvh()
{
	// Clear the top-level BVH buffer
	_fatBuffer.clear();
	_fatPoolPtr = 0;

	// Add the scene graph nodes to teh top-level BVH buffer
	std::vector<u32> list;
	std::stack<std::pair<SceneNode*, glm::mat4>> nodeStack;
	nodeStack.push(std::make_pair(&_scene.get_root_node(), glm::mat4()));
	while (!nodeStack.empty())
	{
		std::pair<SceneNode*, glm::mat4> currentPair = nodeStack.top(); nodeStack.pop();
		SceneNode* sceneNode = currentPair.first;
		glm::mat4 matrix = sceneNode->transform.matrix();
		glm::mat4 transform = currentPair.second * matrix;

		// Visit children
		for (uint i = 0; i < sceneNode->children.size(); i++)
		{
			nodeStack.push(std::make_pair(sceneNode->children[i].get(), transform));
		}

		if (sceneNode->meshData.triangle_count > 0)
		{
			_fatBuffer.push_back(createFatNode(sceneNode, transform));
			list.push_back(_fatPoolPtr++);
		}
	}


	// Slide 50: http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2004%20-%20real-time%20ray%20tracing.pdf
	// Fast Agglomerative Clustering for Rendering (Walter et al, 2008)
	u32 nodeA = list.back();// list.pop_back();
	u32 nodeB = findBestMatch(list, nodeA);
	while (list.size() > 1)
	{
		u32 nodeC = findBestMatch(list, nodeB);
		if (nodeA == nodeC)
		{
			// http://stackoverflow.com/questions/39912/how-do-i-remove-an-item-from-a-stl-vector-with-a-certain-value
			auto newEndA = std::remove(list.begin(), list.end(), nodeA);
			list.erase(newEndA);
			auto newEndB = std::remove(list.begin(), list.end(), nodeB);
			list.erase(newEndB);
			
			// A = new Node(A, B);
			_fatBuffer.push_back(mergeFatNodes(nodeA, nodeB));
			nodeA = _fatPoolPtr++;

			list.push_back(nodeA);
			nodeB = findBestMatch(list, nodeA);
		}
		else {
			nodeA = nodeB;
			nodeB = nodeC;
		}
	}

	/*std::cout << std::endl;
	for (FatBvhNode& node : _fatBuffer)
	{
		if (!node.isLeaf)
			std::cout << "Top level node children: " << node.leftChildIndex << " and " << node.rightChildIndex << std::endl;
		else
			std::cout << "Top level leaf sub-bvh root: " << node.thinBvh << std::endl;
	}*/
}

u32 raytracer::Bvh::allocateThinNode()
{
	_thinBuffer.emplace_back();
	return _thinPoolPtr++;
}

u32 raytracer::Bvh::findBestMatch(const std::vector<u32>& list, u32 nodeId)
{
	FatBvhNode& node = _fatBuffer[nodeId];
	float curMinArea = std::numeric_limits<float>::max();
	u32 curMin = -1;
	for (u32 otherNodeId : list)
	{
		FatBvhNode& otherNode = _fatBuffer[otherNodeId];
		AABB bounds = calcCombinedBounds(node.bounds, otherNode.bounds);
		float leftArea = bounds.extents.z * bounds.extents.y;
		float topArea = bounds.extents.x * bounds.extents.y;
		float backArea = bounds.extents.x * bounds.extents.z;
		float totalArea = leftArea + topArea + backArea;

		if (totalArea < curMinArea && otherNodeId != nodeId)
		{
			curMin = otherNodeId;
			curMinArea = totalArea;
		}
	}

	return curMin;
}

FatBvhNode raytracer::Bvh::createFatNode(const SceneNode* sceneGraphNode, const glm::mat4 transform)
{
	FatBvhNode node;
	node.thinBvh = sceneGraphNode->meshData.thinBvhIndex;
	node.bounds = calcTransformedAABB(_thinBuffer[node.thinBvh].bounds, transform);
	node.invTransform = glm::inverse(transform);
	node.isLeaf = true;
	return node;
}

FatBvhNode raytracer::Bvh::mergeFatNodes(u32 nodeId1, u32 nodeId2)
{
	const FatBvhNode& node1 = _fatBuffer[nodeId1];
	const FatBvhNode& node2 = _fatBuffer[nodeId2];

	FatBvhNode node;
	node.bounds = calcCombinedBounds(node1.bounds, node2.bounds);
 	node.invTransform = glm::mat4();// Identity
	node.leftChildIndex = nodeId1;
	node.rightChildIndex = nodeId2;
	node.isLeaf = false;
	return node;
}

AABB raytracer::Bvh::calcCombinedBounds(const AABB& bounds1, const AABB& bounds2)
{
	glm::vec3 min1 = bounds1.centre - bounds1.extents;
	glm::vec3 min2 = bounds2.centre - bounds2.extents;

	glm::vec3 max1 = bounds1.centre + bounds1.extents;
	glm::vec3 max2 = bounds2.centre + bounds2.extents;

	glm::vec3 min = glm::min(min1, min2);
	glm::vec3 max = glm::max(max1, max2);

	AABB result;
	result.centre = (min + max) / 2.0f;
	result.extents = (max - min) / 2.0f;
	return result;
}

AABB raytracer::Bvh::calcTransformedAABB(const AABB& bounds, glm::mat4 transform)
{
	glm::vec3 min = bounds.centre - bounds.extents;
	glm::vec3 max = bounds.centre + bounds.extents;

	glm::vec3 corners[8];
	corners[0] = glm::vec3(transform * glm::vec4(max.x, max.y, max.z, 1.0f));
	corners[1] = glm::vec3(transform * glm::vec4(max.x, max.y, min.z, 1.0f));
	corners[2] = glm::vec3(transform * glm::vec4(max.x, min.y, max.z, 1.0f));
	corners[3] = glm::vec3(transform * glm::vec4(max.x, min.y, min.z, 1.0f));
	corners[4] = glm::vec3(transform * glm::vec4(min.x, min.y, min.z, 1.0f));
	corners[5] = glm::vec3(transform * glm::vec4(min.x, min.y, max.z, 1.0f));
	corners[6] = glm::vec3(transform * glm::vec4(min.x, max.y, min.z, 1.0f));
	corners[7] = glm::vec3(transform * glm::vec4(min.x, max.y, max.z, 1.0f));

	glm::vec3 newMin = corners[0];
	glm::vec3 newMax = corners[0];
	for (int i = 1; i < 8; i++)
	{
		newMin = glm::min(corners[i], newMin);
		newMax = glm::max(corners[i], newMax);
	}
	
	AABB result;
	result.centre = (newMin + newMax) / 2.0f;
	result.extents = (newMax - newMin) / 2.0f;
	return result;
}

void  raytracer::Bvh::subdivide(ThinBvhNode& node) {
	if (node.triangleCount < 4)
		return;

	u32 leftChildIndex = allocateThinNode();
	u32 rightChildIndex = allocateThinNode();
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