#include "binned_bvh.h"
#include "allocator_singletons.h"
#include <array>
#include <numeric>
#include <algorithm>

#define BVH_SPLITS 8

using namespace raytracer;

u32 raytracer::BinnedBvhBuilder::build(u32 firstTriangle, u32 triangleCount)
{
	_triangleOffset = firstTriangle;
	_centres.resize(triangleCount);
	_aabbs.resize(triangleCount);

	auto& vertices = getVertexAllocatorInstance();
	auto& faces = getTriangleAllocatorInstance();
	auto& bvhAllocator = getSubBvhAllocatorInstance();

	// Calculate centroids
	for (uint i = 0; i < triangleCount; ++i) {
		std::array<glm::vec3, 3> face;
		face[0] = (glm::vec3) vertices.get(faces.get(firstTriangle + i).indices[0]).vertex;
		face[1] = (glm::vec3) vertices.get(faces.get(firstTriangle + i).indices[1]).vertex;
		face[2] = (glm::vec3) vertices.get(faces.get(firstTriangle + i).indices[2]).vertex;
		_centres[i] = std::accumulate(std::begin(face), std::end(face), glm::vec3()) / 3.f;
	}

	// Calculate AABBs
	for (uint i = 0; i < triangleCount; ++i) {
		_aabbs[i] = createBounds(firstTriangle + i);
	}

	u32 rootIndex = allocateNodePair();
	auto& root = bvhAllocator.get(rootIndex);
	root.firstTriangleIndex = firstTriangle;
	root.triangleCount = triangleCount;

	{// Calculate AABB of the root node
		glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());// Work around bug in glm
		glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());// Work around bug in glm
		for (uint i = 0; i < triangleCount; i++)
		{
			min = glm::min(_aabbs[i].min, min);
			max = glm::max(_aabbs[i].max, max);
		}
		root.bounds.min = min;
		root.bounds.max = max;
	}

	// Subdivide the root node
	subdivide(rootIndex);

	return rootIndex;
}

u32 raytracer::BinnedBvhBuilder::allocateNodePair()
{
	auto& bvhAllocator = getSubBvhAllocatorInstance();

	u32 first = bvhAllocator.allocate();
	bvhAllocator.allocate();// Second child
	return first;
}

void raytracer::BinnedBvhBuilder::subdivide(u32 nodeId)
{
	auto& bvhAllocator = getSubBvhAllocatorInstance();

	if (bvhAllocator.get(nodeId).triangleCount < 4)
		return;

	if (partition(nodeId)) {// Divides our triangles over our children and calculates their bounds
		u32 x = bvhAllocator.get(nodeId).leftChildIndex;
		_ASSERT(x > nodeId);
		subdivide(bvhAllocator.get(nodeId).leftChildIndex);
		subdivide(bvhAllocator.get(nodeId).leftChildIndex + 1);
	}
}

bool raytracer::BinnedBvhBuilder::partition(u32 nodeId)
{
	auto& faces = getTriangleAllocatorInstance();
	auto& bvhAllocator = getSubBvhAllocatorInstance();

	// Use pointer because references are not reassignable (we need to reassign after allocation)
	auto* node = &bvhAllocator.get(nodeId);

	// Split along the widest axis
	uint axis = -1;
	float minAxisWidth = 0.0;
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

	u32 localFirstTriangleIndex = node->firstTriangleIndex - _triangleOffset;

	// Loop through the triangles and calculate bin dimensions and triangle count
	const u32 end = localFirstTriangleIndex + node->triangleCount;
	float k1 = BVH_SPLITS / extents[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)
	for (u32 i = localFirstTriangleIndex; i < end; i++)
	{
		// Calculate the bin ID as described in the paper
		float x = k1 * (_centres[i][axis] - node->bounds.min[axis]);
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
			std::swap(faces.get(_triangleOffset + i), faces.get(_triangleOffset + j));
			std::swap(_centres[i], _centres[j]);
			std::swap(_aabbs[i], _aabbs[j]);
			i++;
			j--;
		}
	}

	// Allocate child nodes
	u32 leftIndex = allocateNodePair();
	node = &bvhAllocator.get(nodeId);// Allocation currently uses a std::vector, which means
	// that addresses may change after allocation

	// Initialize child nodes
	auto& lNode = bvhAllocator.get(leftIndex);
	lNode.firstTriangleIndex = node->firstTriangleIndex;
	lNode.triangleCount = 0;
	lNode.bounds = AABB();
	for (int bin = 0; bin < bestSplit; bin++)
	{
		lNode.triangleCount += binTriangleCount[bin];
		if (binTriangleCount[bin] > 0)
			lNode.bounds.fit(binAABB[bin]);
	}
	_ASSERT(lNode.firstTriangleIndex >= _triangleOffset);

	auto& rNode = bvhAllocator.get(leftIndex + 1);
	rNode.firstTriangleIndex = node->firstTriangleIndex + lNode.triangleCount;
	rNode.triangleCount = node->triangleCount - lNode.triangleCount;
	rNode.bounds = AABB();
	for (int bin = bestSplit; bin < BVH_SPLITS; bin++)
	{
		if (binTriangleCount[bin] > 0)
			rNode.bounds.fit(binAABB[bin]);
	}
	_ASSERT(rNode.firstTriangleIndex >= _triangleOffset);

	// set this node as not a leaf anymore
	node->triangleCount = 0;
	node->leftChildIndex = leftIndex;
	_ASSERT(bvhAllocator.get(nodeId).leftChildIndex == leftIndex);
	_ASSERT(node->leftChildIndex > nodeId);

	return true;// Yes we have split, in the future you may want to decide whether you split or not based on the SAH
}

AABB raytracer::BinnedBvhBuilder::createBounds(u32 triangleIndex)
{
	auto& vertices = getVertexAllocatorInstance();
	auto& faces = getTriangleAllocatorInstance();

	glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());// Work around bug in glm
	glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());// Work around bug in glm

	std::array<glm::vec3, 3> face;
	face[0] = (glm::vec3) vertices.get(faces.get(triangleIndex).indices[0]).vertex;
	face[1] = (glm::vec3) vertices.get(faces.get(triangleIndex).indices[1]).vertex;
	face[2] = (glm::vec3) vertices.get(faces.get(triangleIndex).indices[2]).vertex;
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
