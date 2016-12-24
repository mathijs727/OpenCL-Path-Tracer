#include "sbvh.h"
#include <numeric>
#include <array>
#include <stack>
#include "bvh_nodes.h"
#include <xutility>

#define BVH_SPLITS 8

u32 raytracer::SbvhBuilder::build(
	std::vector<VertexSceneData>& vertices,
	std::vector<TriangleSceneData>& triangles,
	std::vector<SubBvhNode>& outBvhNodes)
{
	u32 triangleCount = triangles.size();
	_triangles = &triangles;
	_vertices = &vertices;
	_bvh_nodes = &outBvhNodes;
	_triangleOffset = 0;
	_centres.resize(triangleCount);
	_aabbs.resize(triangleCount);
	_secondaryIndexBuffer.resize(triangleCount);

	for (u32 i = 0; i < triangleCount; ++i) {
		_secondaryIndexBuffer[i] = i;
	}

	// Calculate centroids
	for (uint i = 0; i < triangleCount; ++i) {
		std::array<glm::vec3, 3> face;
		auto& triangle = (*_triangles)[_triangleOffset + i];
		face[0] = (glm::vec3) vertices[triangle.indices[0]].vertex;
		face[1] = (glm::vec3) vertices[triangle.indices[1]].vertex;
		face[2] = (glm::vec3) vertices[triangle.indices[2]].vertex;
		_centres[i] = std::accumulate(face.cbegin(), face.cend(), glm::vec3()) / 3.f;
	}

	// Calculate AABBs
	for (uint i = 0; i < triangleCount; ++i) {
		_aabbs[i] = createBounds(i);
	}

	u32 rootIndex = allocateNodePair();
	auto& root = (*_bvh_nodes)[rootIndex];
	root.firstTriangleIndex = 0; // _triangleOffset;
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

	std::vector<TriangleSceneData> newFaces;
	newFaces.reserve(_secondaryIndexBuffer.size());
	for (u32 i : _secondaryIndexBuffer) newFaces.push_back((*_triangles)[_triangleOffset + i]);

	*_triangles = std::move(newFaces);

	std::stack<u32> nodeStack;
	nodeStack.push(rootIndex);
	while (!nodeStack.empty()) {
		auto& node = (*_bvh_nodes)[nodeStack.top()]; nodeStack.pop();
		if (node.triangleCount == 0) {
			nodeStack.push(node.leftChildIndex);
			nodeStack.push(node.leftChildIndex + 1);
		}
		else {
			node.firstTriangleIndex += _triangleOffset;
		}
	}

	return rootIndex;
}

void raytracer::SbvhBuilder::subdivide(u32 nodeId)
{
	if ((*_bvh_nodes)[nodeId].triangleCount < 4)
		return;

	if (partition(nodeId)) {// Divides our triangles over our children and calculates their bounds
		u32 leftIndex = (*_bvh_nodes)[nodeId].leftChildIndex;
		_ASSERT(leftIndex > nodeId);
		subdivide(leftIndex);
		subdivide(leftIndex + 1);
	}
}

bool raytracer::SbvhBuilder::partition(u32 nodeId)
{
	// Use pointer because references are not reassignable (we need to reassign after allocation)
	auto* node = &(*_bvh_nodes)[nodeId];

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
	
	u32 localFirstTriangleIndex = node->firstTriangleIndex;
	// Loop through the triangles and calculate bin dimensions and triangle count
	const u32 end = localFirstTriangleIndex + node->triangleCount;
	float k1 = BVH_SPLITS / size[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)

	u32 bestBinnedSplit = -1;
	float bestBinnedSah;
	ObjectBin objectBins[BVH_SPLITS];
	doObjectSelection(node, axis, bestBinnedSplit, bestBinnedSah, objectBins);

	u32 bestSpatialSplit = -1;
	float bestSpatialSah;
	SpatialSplit spatialSplits[BVH_SPLITS];
	//doSpatialSelection(node, axis, bestSpatialSplit, bestSpatialSah, spatialSplits);
	
	u32 leftCount, rightCount;
	AABB leftBounds, rightBounds;
	if (bestBinnedSplit != -1 && (bestSpatialSplit == -1 || bestSpatialSah >= bestBinnedSah)) {
		// Partition the array around the bin pivot
		// http://www.inf.fh-flensburg.de/lang/algorithmen/sortieren/quick/quicken.htm
		u32 i = localFirstTriangleIndex;
		u32 j = end - 1;
		while (i < j)
		{
			// Calculate the bin ID as described in the paper
			while (true)
			{
				int bin = static_cast<int>(k1 * (_centres[_secondaryIndexBuffer[i]][axis] - node->bounds.min[axis]));
				bin = glm::min(glm::max(0, bin), BVH_SPLITS - 1); // need to clamp because some points might be outside of the bounding box because of how sbvh works
				if (bin < bestBinnedSplit && i < end - 1)
					i++;
				else
					break;
			}

			while (true)
			{
				int bin = static_cast<int>(k1 * (_centres[_secondaryIndexBuffer[j]][axis] - node->bounds.min[axis]));
				bin = glm::min(glm::max(0, bin), BVH_SPLITS - 1);
				if (bin >= bestBinnedSplit && j > 0)
					j--;
				else
					break;
			}

			if (i <= j)
			{
				// Swap
				std::swap(_secondaryIndexBuffer[i], _secondaryIndexBuffer[j]);
				i++;
				j--;
			}
		}

		leftCount = 0;
		rightCount = 0;
		for (int bin = bestBinnedSplit; bin < BVH_SPLITS; bin++)
		{
			rightCount += objectBins[bin].triangleCount;
			if (objectBins[bin].triangleCount > 0)
				rightBounds.fit(objectBins[bin].bounds);
		}

		for (int bin = 0; bin < bestBinnedSplit; bin++)
		{
			leftCount += objectBins[bin].triangleCount;
			if (objectBins[bin].triangleCount > 0)
				leftBounds.fit(objectBins[bin].bounds);
		}
	}
	else if (bestSpatialSplit != -1) {
		// allocate enough memory to hold the new faces of this node
		leftCount = 0;
		rightCount = 0;
		std::vector<u32> newTriangles;
		for (u32 splitn = 0; splitn < BVH_SPLITS; ++splitn) {
			auto& split = spatialSplits[splitn];
			for (auto ref : split.refs) {
				newTriangles.push_back(ref.triangleIndex);
			}
			if (splitn < bestSpatialSplit) {
				leftCount+= split.refs.size();
				leftBounds.fit(split.bounds);
			}
			else {
				rightCount+= split.refs.size();
				rightBounds.fit(split.bounds);
			}
		}
		u32 nodeTriangleCount = node->triangleCount;
		_ASSERT(newTriangles.size() >= nodeTriangleCount);
		u32 previousSize = _secondaryIndexBuffer.size();
		
		_secondaryIndexBuffer.insert(_secondaryIndexBuffer.begin() + end, newTriangles.size() - nodeTriangleCount, 0);
		//faces.allocateInPlace(_triangleOffset + end, );
		_ASSERT(newTriangles.size() == leftCount + rightCount);
		_ASSERT(_secondaryIndexBuffer.size() - previousSize == newTriangles.size() - nodeTriangleCount);

		//for (int i = localFirstTriangleIndex; i < end; ++i) _secondaryIndexBuffer[i] = newTriangles[i - localFirstTriangleIndex];

		//memcpy(&_secondaryIndexBuffer[localFirstTriangleIndex], newTriangles.data(), newTriangles.size() * sizeof(u32));
		std::copy(newTriangles.begin(), newTriangles.end(), _secondaryIndexBuffer.begin() + localFirstTriangleIndex);
	}
	else {
		return false; // found no subdivision
	}

	// Allocate child nodes
	u32 leftIndex = allocateNodePair();
	node = &(*_bvh_nodes)[nodeId];	// Allocation currently uses a std::vector, which means
									 // that addresses may change after allocation

									 // Initialize child nodes
	auto& lNode = (*_bvh_nodes)[leftIndex];
	lNode.firstTriangleIndex = node->firstTriangleIndex;
	lNode.triangleCount = leftCount;
	lNode.bounds = leftBounds;
	
	_ASSERT(lNode.firstTriangleIndex >= 0);

	auto& rNode = (*_bvh_nodes)[leftIndex + 1];
	rNode.firstTriangleIndex = node->firstTriangleIndex + leftCount;
	rNode.triangleCount = rightCount;
	rNode.bounds = rightBounds;
	
	_ASSERT(rNode.firstTriangleIndex >= 0);

	// set this node as not a leaf anymore
	node->triangleCount = 0;
	node->leftChildIndex = leftIndex;
	_ASSERT((*_bvh_nodes)[nodeId].leftChildIndex == leftIndex);
	_ASSERT(node->leftChildIndex > nodeId);

	return true;// Yes we have split, in the future you may want to decide whether you split or not based on the SAH
}

raytracer::AABB raytracer::SbvhBuilder::createBounds(u32 triangleIndex)
{
	glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());// Work around bug in glm
	glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());// Work around bug in glm

	std::array<glm::vec3, 3> face;
	face[0] = (glm::vec3) (*_vertices)[(*_triangles)[triangleIndex].indices[0]].vertex;
	face[1] = (glm::vec3) (*_vertices)[(*_triangles)[triangleIndex].indices[1]].vertex;
	face[2] = (glm::vec3) (*_vertices)[(*_triangles)[triangleIndex].indices[2]].vertex;
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
	u32 index = _bvh_nodes->size();
	_bvh_nodes->emplace_back();
	_bvh_nodes->emplace_back();
	return index;
}

bool raytracer::SbvhBuilder::doObjectSelection(SubBvhNode* node, u32 axis, u32& outBestSplit, float& outBestSah, ObjectBin* bins)
{
	for (int i = 0; i < BVH_SPLITS; i++)
		bins[i].triangleCount = 0;

	u32 localFirstTriangleIndex = node->firstTriangleIndex;

	glm::vec3 extents = node->bounds.max - node->bounds.min;
	// Loop through the triangles and calculate bin dimensions and triangle count
	const u32 end = localFirstTriangleIndex + node->triangleCount;
	float k1 = BVH_SPLITS / extents[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)
	for (u32 i = localFirstTriangleIndex; i < end; i++)
	{
		// Calculate the bin ID as described in the paper
		float x = k1 * (_centres[_secondaryIndexBuffer[i]][axis] - node->bounds.min[axis]);
		int bin = static_cast<int>(x);

		bins[bin].triangleCount++;
		bins[bin].bounds.fit(_aabbs[_secondaryIndexBuffer[i]]);
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
				triangleCountLeft += bins[leftBin].triangleCount;
				if (bins[leftBin].triangleCount > 0)
					leftAABB.fit(bins[leftBin].bounds);
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
				triangleCountRight += bins[rightBin].triangleCount;
				if (bins[rightBin].triangleCount > 0)
					rightAABB.fit(bins[rightBin].bounds);
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

	outBestSah = bestSAH;
	outBestSplit = bestSplit;

	return bestSplit != 1;
}

bool raytracer::SbvhBuilder::doSpatialSelection(SubBvhNode* node, u32 axis, u32& outBestSplit, float& outBestSah, SpatialSplit* spatialSplits)
{
	glm::vec3 size = node->bounds.max - node->bounds.min;
	u32 localFirstTriangleIndex = node->firstTriangleIndex;
	const u32 end = localFirstTriangleIndex + node->triangleCount;
	float k1 = BVH_SPLITS / size[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)

	// build the bins
	for (u32 i = localFirstTriangleIndex; i < end; i++)
	{
		// Calculate the bin ID for the minimum and maximum points
		float x = k1 * (_aabbs[_secondaryIndexBuffer[i]].min[axis] - node->bounds.min[axis]);
		int bin_min = static_cast<int>(x);
		bin_min = glm::min(glm::max(0, bin_min), BVH_SPLITS - 1);
		float y = k1 * (_aabbs[_secondaryIndexBuffer[i]].max[axis] - node->bounds.min[axis]);
		int bin_max = static_cast<int>(x);
		bin_max = glm::min(glm::max(0, bin_max), BVH_SPLITS - 1);

		// if contained within a single bin don't need clipping
		if (bin_min == bin_max) {
			AABB triangleBounds = _aabbs[_secondaryIndexBuffer[i]];
			SpatialSplitRef newRef;
			newRef.triangleIndex = _secondaryIndexBuffer[i];
			newRef.clippedBounds = triangleBounds;
			spatialSplits[bin_min].refs.push_back(newRef);
			spatialSplits[bin_min].bounds.fit(triangleBounds);
		}
		else {
			// add the triangle to the required bins
			for (int j = bin_min; j <= bin_max; ++j) {
				// TODO: implement better clipping, this is rather imprecise
				AABB triangleBounds = _aabbs[_secondaryIndexBuffer[i]];
				triangleBounds.min[axis] = glm::max(k1 * j, triangleBounds.min[axis]);
				triangleBounds.max[axis] = glm::min(k1 * (j + 1), triangleBounds.min[axis]);

				SpatialSplitRef newRef;
				newRef.triangleIndex = _secondaryIndexBuffer[i];
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

	outBestSah = bestSAH;
	outBestSplit = bestSplit;

	return bestSplit != -1;
}
