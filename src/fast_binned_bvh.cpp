#include "fast_binned_bvh.h"
#include <array>
#include <numeric>
#include <algorithm>
#include <iostream>

#define BVH_SPLITS 32

using namespace raytracer;

u32 raytracer::FastBinnedBvhBuilder::build(
	std::vector<VertexSceneData>& vertices,
	std::vector<TriangleSceneData>& triangles,
	std::vector<SubBvhNode>& outBvhNodes)
{
	_triangles = &triangles;
	_vertices = &vertices;
	_bvh_nodes = &outBvhNodes;

	_centres.clear();
	_aabbs.clear();
	_centres.reserve(triangles.size());
	_aabbs.reserve(triangles.size());

	// Calculate centroids
	for (auto& triangle : triangles) {
		std::array<glm::vec3, 3> face;
		face[0] = (glm::vec3) vertices[(triangle.indices[0])].vertex;
		face[1] = (glm::vec3) vertices[(triangle.indices[1])].vertex;
		face[2] = (glm::vec3) vertices[(triangle.indices[2])].vertex;
		_centres.push_back(std::accumulate(std::begin(face), std::end(face), glm::vec3()) / 3.f);
	}

	// Calculate AABBs
	for (auto& triangle : triangles) {
		_aabbs.push_back(createBounds(triangle));
	}

	u32 rootIndex = allocateNodePair();
	auto& root = (*_bvh_nodes)[rootIndex];
	root.firstTriangleIndex = 0;
	root.triangleCount = (u32)triangles.size();

	{// Calculate AABB of the root node
		glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());// Work around bug in glm
		glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());// Work around bug in glm
		for (auto& aabb : _aabbs)
		{
			min = glm::min(aabb.min, min);
			max = glm::max(aabb.max, max);
		}
		root.bounds.min = min;
		root.bounds.max = max;
	}

	// Subdivide the root node
	subdivide(rootIndex);

	return rootIndex;
}

u32 raytracer::FastBinnedBvhBuilder::allocateNodePair()
{
	u32 index = (u32)_bvh_nodes->size();
	_bvh_nodes->emplace_back();
	_bvh_nodes->emplace_back();
	return index;
}

void raytracer::FastBinnedBvhBuilder::subdivide(u32 nodeId)
{
	if ((*_bvh_nodes)[nodeId].triangleCount < 4)
		return;

	if (partition(nodeId)) {// Divides our triangles over our children and calculates their bounds
		subdivide((*_bvh_nodes)[nodeId].leftChildIndex);
		subdivide((*_bvh_nodes)[nodeId].leftChildIndex + 1);
	}
}

bool raytracer::FastBinnedBvhBuilder::partition(u32 nodeId)
{
	// Use pointer because references are not reassignable (we need to reassign after allocation)
	auto* node = &(*_bvh_nodes)[nodeId];

	// Split along the widest axis
	uint axis = -1;
	float minAxisWidth = 0.0f;
	glm::vec3 extents = node->bounds.max - node->bounds.min;
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

	u32 localFirstTriangleIndex = node->firstTriangleIndex;

	// Loop through the triangles and calculate bin dimensions and triangle count
	const u32 end = localFirstTriangleIndex + node->triangleCount;
	float k1 = BVH_SPLITS / extents[axis] * 0.9999f;// Prevents the bin out of bounds (if centroid on the right bound)
	for (u32 i = localFirstTriangleIndex; i < end; i++)
	{
		// Calculate the bin ID as described in the paper
		float x = k1 * (_centres[i][axis] - node->bounds.min[axis]);
		int bin = static_cast<int>(x);

		binTriangleCount[bin]++;
		binAABB[bin].fit(_aabbs[i]);
	}

	// Determine for which bin the SAH is the lowest
	float bestSAH = std::numeric_limits<float>::max();
	int bestSplit = -1;
	for (int split = 1; split < BVH_SPLITS; split++)
	{
		// Calculate the triangle count and surface area of the AABB to the left of the possible split
		int triangleCountLeft = 0;
		float surfaceAreaLeft = 0.0f;
		{
			AABB leftAABB;
			for (int leftBin = 0; leftBin < split; leftBin++)
			{
				triangleCountLeft += binTriangleCount[leftBin];
				if (binTriangleCount[leftBin] > 0)
					leftAABB.fit(binAABB[leftBin]);
			}
			surfaceAreaLeft = leftAABB.surfaceArea();
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
			surfaceAreaRight = rightAABB.surfaceArea();
		}

		float SAH = triangleCountLeft * surfaceAreaLeft + triangleCountRight * surfaceAreaRight;
		if (SAH < bestSAH && triangleCountLeft > 0 && triangleCountRight > 0)
		{
			bestSAH = SAH;
			bestSplit = split;
		}
	}

	// Dont split if we cannot find a split (if all triangles have there center in one bin for example)
	if (bestSplit == -1)
		return false;

	// Partition the array around the bin pivot
	// http://www.inf.fh-flensburg.de/lang/algorithmen/sortieren/quick/quicken.htm
	u32 i = localFirstTriangleIndex;
	u32 j = end - 1;
	while (i < j)
	{
		// Calculate the bin ID as described in the paper
		while (true)
		{
			int bin = static_cast<int>(k1 * (_centres[i][axis] - node->bounds.min[axis]));
			if (bin < bestSplit && i < end - 1)
				i++;
			else
				break;
		}

		while (true)
		{
			int bin = static_cast<int>(k1 * (_centres[j][axis] - node->bounds.min[axis]));
			if (bin >= bestSplit && j > 0)
				j--;
			else
				break;
		}

		if (i <= j)
		{
			// Swap
			std::swap((*_triangles)[i], (*_triangles)[j]);
			std::swap(_centres[i], _centres[j]);
			std::swap(_aabbs[i], _aabbs[j]);
			i++;
			j--;
		}
	}

	// Allocate child nodes
	u32 leftIndex = allocateNodePair();
	node = &(*_bvh_nodes)[nodeId];// Allocation currently uses a std::vector, which means
								  // that addresses may change after allocation

								  // Initialize child nodes
	auto& lNode = (*_bvh_nodes)[leftIndex];
	lNode.firstTriangleIndex = node->firstTriangleIndex;
	lNode.triangleCount = 0;
	lNode.bounds = AABB();
	for (int bin = 0; bin < bestSplit; bin++)
	{
		lNode.triangleCount += binTriangleCount[bin];
		if (binTriangleCount[bin] > 0)
			lNode.bounds.fit(binAABB[bin]);
	}

	auto& rNode = (*_bvh_nodes)[leftIndex + 1];
	rNode.firstTriangleIndex = node->firstTriangleIndex + lNode.triangleCount;
	rNode.triangleCount = node->triangleCount - lNode.triangleCount;
	rNode.bounds = AABB();
	for (int bin = bestSplit; bin < BVH_SPLITS; bin++)
	{
		if (binTriangleCount[bin] > 0)
			rNode.bounds.fit(binAABB[bin]);
	}

	// set this node as not a leaf anymore
	node->triangleCount = 0;
	node->leftChildIndex = leftIndex;

	return true;// Yes we have split, in the future you may want to decide whether you split or not based on the SAH
}

AABB raytracer::FastBinnedBvhBuilder::createBounds(const TriangleSceneData& triangle)
{
	glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());// Work around bug in glm
	glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());// Work around bug in glm

	std::array<glm::vec3, 3> face;
	face[0] = (glm::vec3) (*_vertices)[triangle.indices[0]].vertex;
	face[1] = (glm::vec3) (*_vertices)[triangle.indices[1]].vertex;
	face[2] = (glm::vec3) (*_vertices)[triangle.indices[2]].vertex;
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

