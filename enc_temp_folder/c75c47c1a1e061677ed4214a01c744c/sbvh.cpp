#include "sbvh.h"
#include "allocator_singletons.h"
#include <numeric>
#include <array>
#include "linear_allocator.h"

#define BVH_SPLITS 8

u32 raytracer::SbvhBuilder::build(u32 firstTriangle, u32 triangleCount)
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
		auto& triangle = faces[firstTriangle + i];
		face[0] = (glm::vec3) vertices[triangle.indices[0]].vertex;
		face[1] = (glm::vec3) vertices[triangle.indices[1]].vertex;
		face[2] = (glm::vec3) vertices[triangle.indices[2]].vertex;
		_centres[i] = std::accumulate(face.cbegin(), face.cend(), glm::vec3()) / 3.f;
	}

	// Calculate AABBs
	for (uint i = 0; i < triangleCount; ++i) {
		_aabbs[i] = createBounds(firstTriangle + i);
	}

	u32 rootIndex = allocateNodePair();
	auto& root = bvhAllocator[rootIndex];
	root.firstTriangleIndex = 0;
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

void raytracer::SbvhBuilder::subdivide(u32 nodeId)
{
	auto& bvhAllocator = getSubBvhAllocatorInstance();

	if (bvhAllocator[nodeId].triangleCount < 4)
		return;

	if (partition(nodeId)) {// Divides our triangles over our children and calculates their bounds
		u32 leftIndex = bvhAllocator[nodeId].leftChildIndex;
		_ASSERT(leftIndex > nodeId);
		subdivide(leftIndex);
		subdivide(leftIndex + 1);
	}
}

bool raytracer::SbvhBuilder::partition(u32 nodeId)
{
	auto& faces = getTriangleAllocatorInstance();
	auto& vertices = getVertexAllocatorInstance();
	auto& bvhAllocator = getSubBvhAllocatorInstance();

	// Use pointer because references are not reassignable (we need to reassign after allocation)
	auto* node = &bvhAllocator[nodeId];

	// Split along the widest axis
	uint axis = -1;
	float minAxisWidth = 0.0;
	glm::vec3 size = node->bounds.max - node->bounds.min;
	for (int i = 0; i < 3; i++)
	{
		if (size[i] > minAxisWidth)
		{
			minAxisWidth = size[i];
			axis = i;
		}
	}

	u32 localFirstTriangleIndex = node->firstTriangleIndex - _triangleOffset;
	// Loop through the triangles and calculate bin dimensions and triangle count
	const u32 end = localFirstTriangleIndex + node->triangleCount;

	u32 bestBinnedSplit;
	float bestBinnedSah;
	std::array<uint, BVH_SPLITS> binTriangleCount;
	std::array<AABB, BVH_SPLITS> binAABB;
	float k1 = BVH_SPLITS / size[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)

	// do the binned selection
	{
		for (int i = 0; i < BVH_SPLITS; i++)
			binTriangleCount[i] = 0;
		
		for (u32 i = localFirstTriangleIndex; i < end; i++)
		{
			// Calculate the bin ID as described in the paper
			float x = k1 * (_centres[i][axis] - node->bounds.min[axis]);
			int bin = glm::min(glm::max(0,static_cast<int>(x)), BVH_SPLITS-1); // need to clamp because some points might be outside of the bounding box because of how sbvh works

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

		bestBinnedSah = bestSAH;
		bestBinnedSplit = bestSplit;
	}

	u32 bestSpatialSplit;
	float bestSpatialSah;
	
	struct SpatialSplitRef {
		u32 triangleIndex;
		AABB clippedBounds;
	};

	struct SpatialSplit {
		AABB bounds;
		std::vector<SpatialSplitRef> refs;
	};

	std::array<SpatialSplit, BVH_SPLITS> spatialSplits;
	
	// do the spatial split selection
	{
		// build the bins
		for (u32 i = localFirstTriangleIndex; i < end; i++)
		{
			// Calculate the bin ID for the minimum and maximum points
			float x = k1 * (_aabbs[i].min[axis] - node->bounds.min[axis]);
			int bin_min = static_cast<int>(x);
			bin_min = glm::min(glm::max(0, bin_min), BVH_SPLITS - 1);
			float y = k1 * (_aabbs[i].max[axis] - node->bounds.min[axis]);
			int bin_max = static_cast<int>(x);
			bin_max = glm::min(glm::max(0, bin_max), BVH_SPLITS - 1);

			// if contained within a single bin don't need clipping
			if (bin_min == bin_max) {
				AABB triangleBounds = _aabbs[i];
				SpatialSplitRef newRef;
				newRef.triangleIndex = i;
				newRef.clippedBounds = triangleBounds;
				spatialSplits[bin_min].refs.push_back(newRef);
				spatialSplits[bin_min].bounds.fit(triangleBounds);
			}
			else {
				// add the triangle to the required bins
				for (int j = bin_min; j <= bin_max; ++j) {
					// TODO: implement better clipping, this is rather imprecise
					AABB triangleBounds = _aabbs[i];
					triangleBounds.min[axis] = glm::max(k1 * j, triangleBounds.min[axis]);
					triangleBounds.max[axis] = glm::min(k1 * (j+1), triangleBounds.min[axis]);

					SpatialSplitRef newRef;
					newRef.triangleIndex = i;
					newRef.clippedBounds = triangleBounds;
					spatialSplits[j].refs.push_back(newRef);
					spatialSplits[j].bounds.fit(triangleBounds);
					/*
					auto v1 = vertices[faces[_secondaryIndexBuffer[i]].indices[0]].vertex;
					auto v2 = vertices[faces[_secondaryIndexBuffer[i]].indices[1]].vertex;
					auto v3 = vertices[faces[_secondaryIndexBuffer[i]].indices[2]].vertex;
					auto e1 = v2 - v1;
					auto e2 = v3 - v2;
					auto e3 = v1 - v3;*/
				}
			}
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
					triangleCountLeft += spatialSplits[leftBin].refs.size();
					if (spatialSplits[leftBin].refs.size() > 0)
						leftAABB.fit(spatialSplits[leftBin].bounds);
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
					triangleCountRight += spatialSplits[rightBin].refs.size();
					if (spatialSplits[rightBin].refs.size() > 0)
						rightAABB.fit(spatialSplits[rightBin].bounds);
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

		bestSpatialSah = bestSAH;
		bestSpatialSplit = bestSplit;
	}
	
	u32 leftCount, rightCount;
	AABB leftBounds, rightBounds;
	if (bestBinnedSplit != -1 && bestSpatialSah <= bestBinnedSah) {
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
				bin = glm::min(glm::max(0, bin), BVH_SPLITS - 1); // need to clamp because some points might be outside of the bounding box because of how sbvh works
				if (bin < bestBinnedSplit && i < end - 1)
					i++;
				else
					break;
			}

			while (true)
			{
				int bin = static_cast<int>(k1 * (_centres[j][axis] - node->bounds.min[axis]));
				bin = glm::min(glm::max(0, bin), BVH_SPLITS - 1);
				if (bin >= bestBinnedSplit && j > 0)
					j--;
				else
					break;
			}

			if (i <= j)
			{
				// Swap
				std::swap(faces[_triangleOffset + i], faces[_triangleOffset + j]);
				std::swap(_centres[i], _centres[j]);
				std::swap(_aabbs[i], _aabbs[j]);
				i++;
				j--;
			}
		}

		leftCount = 0;
		rightCount = 0;
		for (int bin = bestBinnedSplit; bin < BVH_SPLITS; bin++)
		{
			rightCount += binTriangleCount[bin];
			if (binTriangleCount[bin] > 0)
				rightBounds.fit(binAABB[bin]);
		}

		for (int bin = 0; bin < bestBinnedSplit; bin++)
		{
			leftCount += binTriangleCount[bin];
			if (binTriangleCount[bin] > 0)
				leftBounds.fit(binAABB[bin]);
		}
	}
	else if (bestSpatialSplit != -1) {
		// allocate enough memory to hold the new faces of this node
		leftCount = 0;
		rightCount = 0;
		std::vector<TriangleSceneData> newTriangles;
		for (u32 splitn = 0; splitn < BVH_SPLITS; ++splitn) {
			auto& split = spatialSplits[splitn];
			for (auto ref : split.refs) {
				newTriangles.push_back(faces[ref.triangleIndex + _triangleOffset]);
			}
			if (splitn < bestSpatialSplit) {
				leftCount++;
				leftBounds.fit(split.bounds);
			}
			else {
				rightCount++;
				rightBounds.fit(split.bounds);
			}
		}
		_ASSERT(newTriangles.size() >= node->triangleCount);
		faces.allocateInPlace(_triangleOffset + end, newTriangles.size() - node->triangleCount);
		std::copy(newTriangles.begin(), newTriangles.end(), faces.data() + _triangleOffset + localFirstTriangleIndex);
	}
	else {
		return false; // found no subdivision
	}

	// Allocate child nodes
	u32 leftIndex = allocateNodePair();
	node = &bvhAllocator[nodeId];	// Allocation currently uses a std::vector, which means
									 // that addresses may change after allocation

									 // Initialize child nodes
	auto& lNode = bvhAllocator[leftIndex];
	lNode.firstTriangleIndex = node->firstTriangleIndex;
	lNode.triangleCount = leftCount;
	lNode.bounds = leftBounds;
	
	_ASSERT(lNode.firstTriangleIndex >= _triangleOffset);

	auto& rNode = bvhAllocator[leftIndex + 1];
	rNode.firstTriangleIndex = node->firstTriangleIndex + leftCount;
	rNode.triangleCount = rightCount;
	rNode.bounds = rightBounds;
	
	_ASSERT(rNode.firstTriangleIndex >= _triangleOffset);

	// set this node as not a leaf anymore
	node->triangleCount = 0;
	node->leftChildIndex = leftIndex;
	_ASSERT(bvhAllocator[nodeId].leftChildIndex == leftIndex);
	_ASSERT(node->leftChildIndex > nodeId);

	return true;// Yes we have split, in the future you may want to decide whether you split or not based on the SAH
}

raytracer::AABB raytracer::SbvhBuilder::createBounds(u32 triangleIndex)
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

u32 raytracer::SbvhBuilder::allocateNodePair()
{
	auto& bvhAllocator = getSubBvhAllocatorInstance();

	u32 first = bvhAllocator.allocate();
	bvhAllocator.allocate();// Second child
	return first;
}
