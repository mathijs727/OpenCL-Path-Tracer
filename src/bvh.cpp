#include "bvh.h"
#include "scene.h"
#include <stack>
#include <numeric>
#include <array>
#include <algorithm>
#include <crtdbg.h>

using namespace raytracer;

#define BVH_SPLITS 8

void raytracer::Bvh::buildThinBvhs() {
	_thinBuffer.clear();
	_thinPoolPtr = 0;

	_centres.resize(_scene._triangle_indices.size());
	_aabbs.resize(_scene._triangle_indices.size());

	// Calculate centroids
	for (uint i = 0; i < _scene._triangle_indices.size(); ++i) {
		auto& triang = _scene.GetTriangleIndices()[i];
		std::array<glm::vec3, 3> vertices;
		vertices[0] = (glm::vec3) _scene.GetVertices()[triang.indices[0]].vertex;
		vertices[1] = (glm::vec3) _scene.GetVertices()[triang.indices[1]].vertex;
		vertices[2] = (glm::vec3) _scene.GetVertices()[triang.indices[2]].vertex;
		_centres[i] = std::accumulate(std::begin(vertices), std::end(vertices), glm::vec3()) / 3.f;
	}

	// Calculate AABBs
	for (uint i = 0; i < _scene._triangle_indices.size(); ++i) {
		_aabbs[i] = create_bounds(i);
	}

	std::stack<SceneNode*> node_stack;
	node_stack.push(&_scene.get_root_node());
	while (!node_stack.empty()) {
		SceneNode& current = *node_stack.top(); node_stack.pop();
		
		// Visit children
		for (uint i = 0; i < current.children.size(); i++) {
			node_stack.push(current.children[i].get());
		}
		
		u32 startTriangleIndex = current.meshData.start_triangle_index;
		u32 triangleCount = current.meshData.triangle_count;
		if (triangleCount < 1)
			continue;

		_thinBuffer.reserve(_thinBuffer.size() + triangleCount*2);

		u32 rootIndex = allocateThinNodePair();
		current.meshData.thinBvhIndex = rootIndex;
		auto& root = _thinBuffer[rootIndex];
		root.firstTriangleIndex = startTriangleIndex;
		root.triangleCount = triangleCount;

		{// Calculate AABB of the root node
			glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());// Work around bug in glm
			glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());// Work around bug in glm
			for (uint i = startTriangleIndex; i < startTriangleIndex + triangleCount; i++)
			{
				min = glm::min(_aabbs[i].min, min);
				max = glm::max(_aabbs[i].max, max);
			}
			root.bounds.min = min;
			root.bounds.max = max;
		}

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
}

u32 raytracer::Bvh::allocateThinNodePair()
{
	_thinBuffer.emplace_back();
	_thinBuffer.emplace_back();
	u32 index = _thinPoolPtr;
	_thinPoolPtr += 2;
	return index;
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
		glm::vec3 extents = bounds.max - bounds.min;
		float leftArea = extents.z * extents.y;
		float topArea = extents.x * extents.y;
		float backArea = extents.x * extents.z;
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
	AABB result;
	result.min = glm::min(bounds1.min, bounds2.min);
	result.max = glm::max(bounds1.max, bounds2.max);
	return result;
}

AABB raytracer::Bvh::calcTransformedAABB(const AABB& bounds, glm::mat4 transform)
{
	glm::vec3 min = bounds.min;
	glm::vec3 max = bounds.max;

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
	result.min = newMin;
	result.max = newMax;
	return result;
}

void raytracer::Bvh::subdivide(ThinBvhNode& node) {
	if (node.triangleCount < 4)
		return;

	if (partitionBinned(node)) {// Divides our triangles over our children and calculates their bounds
		subdivide(_thinBuffer[node.leftChildIndex]);
		subdivide(_thinBuffer[node.leftChildIndex + 1]);
	}
}

// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.131.7521&rep=rep1&type=pdf
bool raytracer::Bvh::partitionBinned(ThinBvhNode& node) {
	auto* triangles = _scene.GetTriangleIndices().data();

	// Split along the widest axis
	uint axis = -1;
	float minAxisWidth = 0.0;
	glm::vec3 extents = node.bounds.max - node.bounds.min;
	for (int i = 0; i < 3; i++)
	{
		if (extents[i] > minAxisWidth)
		{
			minAxisWidth = extents[i];
			axis = i;
		}
	}

	std::array<uint, BVH_SPLITS> binTriangleCount;
	std::array<AABB, BVH_SPLITS> binAABB;

	for (int i = 0; i < BVH_SPLITS; i++)
		binTriangleCount[i] = 0;

	// Loop through the triangles and calculate bin dimensions and triangle count
	const u32 end = node.firstTriangleIndex + node.triangleCount;
	float k1 = BVH_SPLITS / extents[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)
	for (u32 i = node.firstTriangleIndex; i < end; i++)
	{
		// Calculate the bin ID as described in the paper
		float x = k1 * (_centres[i][axis] - node.bounds.min[axis]);
		int bin = static_cast<int>(x);

		binTriangleCount[bin]++;
		binAABB[bin].fit(_aabbs[i]);
	}

	// Determine for which bin the SAH is the lowest
	float bestSAH = std::numeric_limits<float>::lowest();
	int bestSplit = -1;
	for (int split = 1; split < BVH_SPLITS; split++)
	{
		// Calculate the triangle count and surface area of the AABB to the left of the possible split
		int triangleCountLeft = 0.0f;
		float surfaceAreaLeft = 0.0f;
		{
			AABB leftAABB;
			for (int leftBin = 0; leftBin < split; leftBin++)
			{
				triangleCountLeft += binTriangleCount[leftBin];
				if (binTriangleCount[leftBin] > 0)
					leftAABB.fit(binAABB[leftBin]);
			}
			glm::vec3 extents = leftAABB.max - leftAABB.min;
			surfaceAreaLeft = 2.0f * (extents.x * extents.y + extents.y * extents.z + extents.z * extents.x);
		}

		// Calculate the triangle count and surface area of the AABB to the right of the possible split
		int triangleCountRight = 0;
		float surfaceAreaRight = 0.0f;
		{
			AABB rightAABB;
			for (int rightBin = split; rightBin < BVH_SPLITS; rightBin++)
			{
				triangleCountRight += binTriangleCount[rightBin];
				if (binTriangleCount[rightBin] > 0)
					rightAABB.fit(binAABB[rightBin]);
			}
			glm::vec3 extents = rightAABB.max - rightAABB.min;
			surfaceAreaRight = 2.0f * (extents.x * extents.y + extents.y * extents.z + extents.z * extents.x);
		}

		float SAH = triangleCountLeft * surfaceAreaLeft + triangleCountRight * surfaceAreaRight;
		if (SAH > bestSAH && triangleCountLeft > 0 && triangleCountRight > 0)
		{
			bestSAH = SAH;
			bestSplit = split;
		}
	}

	// Dont split if we cannot find a split (if all triangles have there center in one bin for example)
	if (bestSplit == -1)
		return false;

	// http://www.inf.fh-flensburg.de/lang/algorithmen/sortieren/quick/quicken.htm
	u32 i = node.firstTriangleIndex;
	u32 j = end - 1;
	while (i < j)
	{
		// Calculate the bin ID as described in the paper
		while (true)
		{
			int bin = static_cast<int>(k1 * (_centres[i][axis] - node.bounds.min[axis]));
			if (bin < bestSplit && i < end - 1)
				i++;
			else
				break;
		}

		while (true)
		{
			int bin = static_cast<int>(k1 * (_centres[j][axis] - node.bounds.min[axis]));
			if (bin >= bestSplit && j > 0)
				j--;
			else
				break;
		}

		if (i <= j)
		{
			// Swap
			std::swap(_scene.GetTriangleIndices()[i], _scene.GetTriangleIndices()[j]);
			std::swap(_centres[i], _centres[j]);
			std::swap(_aabbs[i], _aabbs[j]);
			i++;
			j--;
		}
	}

	// Allocate child nodes
	u32 leftIndex = allocateThinNodePair();

	// Initialize child nodes
	auto& lNode = _thinBuffer[leftIndex];
	lNode.firstTriangleIndex = node.firstTriangleIndex;
	lNode.triangleCount = 0;
	lNode.bounds = AABB();
	for (int bin = 0; bin < bestSplit; bin++)
	{
		lNode.triangleCount += binTriangleCount[bin];
		if (binTriangleCount[bin] > 0)
			lNode.bounds.fit(binAABB[bin]);
	}

	auto& rNode = _thinBuffer[leftIndex + 1];
	rNode.firstTriangleIndex = node.firstTriangleIndex + lNode.triangleCount;
	rNode.triangleCount = node.triangleCount - lNode.triangleCount;
	rNode.bounds = AABB();
	for (int bin = bestSplit; bin < BVH_SPLITS; bin++)
	{
		if (binTriangleCount[bin] > 0)
			rNode.bounds.fit(binAABB[bin]);
	}

	// set this node as not a leaf anymore
	node.triangleCount = 0;
	node.leftChildIndex = leftIndex;

	return true;// Yes we have split, in the future you may want to decide whether you split or not based on the SAH
}

AABB raytracer::Bvh::create_bounds(u32 triangleIndex) {
	glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());// Work around bug in glm
	glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());// Work around bug in glm
	auto& triang = _scene.GetTriangleIndices()[triangleIndex];

	std::array<glm::vec3, 3> vertices;
	vertices[0] = (glm::vec3) _scene.GetVertices()[triang.indices[0]].vertex;
	vertices[1] = (glm::vec3) _scene.GetVertices()[triang.indices[1]].vertex;
	vertices[2] = (glm::vec3) _scene.GetVertices()[triang.indices[2]].vertex;
	for (auto& v : vertices)
	{
		min = glm::min(min, v);
		max = glm::max(max, v);
	}

	AABB result;
	result.min = min;
	result.max = max;
	return result;
}