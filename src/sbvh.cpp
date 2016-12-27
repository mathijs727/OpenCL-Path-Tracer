#include "sbvh.h"
#include <numeric>
#include <array>
#include <stack>
#include "bvh_nodes.h"
#include <xutility>
#include "aabb.h"

#define BVH_SPLITS 8
#define ALPHA 0.01f
#define COST_INTERSECTION 1
#define COST_TRAVERSAL 1

u32 raytracer::SbvhBuilder::build(
	std::vector<VertexSceneData>& vertices,
	std::vector<TriangleSceneData>& triangles,
	std::vector<SubBvhNode>& outBvhNodes)
{
	u32 triangleCount = triangles.size();
	_triangles = &triangles;
	_vertices = &vertices;
	_bvh_nodes = &outBvhNodes;
	_centres.resize(triangleCount);
	_aabbs.resize(triangleCount);
	_secondaryIndexBuffer.resize(triangleCount);

	for (u32 i = 0; i < triangleCount; ++i) {
		_secondaryIndexBuffer[i] = i;
	}

	// Calculate centroids
	for (uint i = 0; i < triangleCount; ++i) {
		std::array<glm::vec3, 3> face;
		auto& triangle = (*_triangles)[i];
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
	for (u32 i : _secondaryIndexBuffer) newFaces.push_back((*_triangles)[i]);

	*_triangles = std::move(newFaces);

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
	//if (node->triangleCount < 4) return false;
	
	// termination criterion. If no sah can be found higher than this it will not split
	float maxSah = (node->triangleCount - (float)COST_TRAVERSAL / COST_INTERSECTION) * node->bounds.surfaceArea();
	
	glm::vec3 size = node->bounds.max - node->bounds.min;
	u32 localFirstTriangleIndex = node->firstTriangleIndex;
	// Loop through the triangles and calculate bin dimensions and triangle count
	const u32 end = localFirstTriangleIndex + node->triangleCount;

	bool spatialSplit = false;
	float bestSah = maxSah;
	int bestAxis = -1;
	FinalSplit bestLeft;
	FinalSplit bestRight;
	std::array<ObjectBin, BVH_SPLITS> objectBinsForEachAxis[3];
	int bestObjectSplit = -1;
	for (u32 axis = 0; axis < 3; ++axis) {
		auto& localObjectBins = objectBinsForEachAxis[axis];
		makeObjectBins(node, axis, localObjectBins.data());
		std::array<SpatialSplit, BVH_SPLITS> spatialBins;
		makeSpatialBins(node, axis, spatialBins.data());

		u32 bestLocalObjectSplit = -1;
		FinalSplit localLeft;
		FinalSplit localRight;
		for (u32 split = 0; split < BVH_SPLITS; split++) {
			float objectSah;
			float spatialSah;
			
			bool objectSplitGood = doSingleObjectSplit(node, axis, split, objectSah, localObjectBins.data()) && (objectSah < bestSah);
			
			AABB leftBounds, rightBounds;
			for (int bin = 0; bin < BVH_SPLITS; ++bin) {
				if (bin < bestLocalObjectSplit) {
					leftBounds.fit(localObjectBins[bin].bounds);
				}
				else {
					rightBounds.fit(localObjectBins[bin].bounds);
				}
			}

			AABB intersection;
			bool intersect = true;
			for (u32 ax = 0; ax < 3; ++ax) {
				intersection.min[ax] = glm::max(leftBounds.min[ax], rightBounds.min[ax]);
				intersection.max[ax] = glm::min(leftBounds.max[ax], rightBounds.max[ax]);
				if (intersection.min[ax] > intersection.max[ax]) intersect = false;
			}
			bool doSpatialSplit = true;
			if (intersect) {
				float heuristic = intersection.surfaceArea() / node->bounds.surfaceArea();
				doSpatialSplit = heuristic > ALPHA;
			}

			FinalSplit left, right;
			bool spatialSplitGood = doSpatialSplit && doSingleSpatialSplit(node, axis, split, spatialSah, left, right, spatialBins.data()) && (spatialSah < bestSah);
			if (objectSplitGood && (!spatialSplitGood || objectSah < spatialSah)) {
				bestSah = objectSah;
				bestLocalObjectSplit = split;
				spatialSplit = false;
				bestAxis = axis;
			}
			else if (spatialSplitGood) {
				bestSah = spatialSah;
				spatialSplit = true;
				localLeft = std::move(left);
				localRight = std::move(right);
				bestAxis = axis;
			}
		}

		// copy results
		if (bestAxis == axis) {
			if (spatialSplit) {
				bestLeft = std::move(localLeft);
				bestRight = std::move(localRight);
			}
			else {
				bestObjectSplit = bestLocalObjectSplit;
			}
		}
	}

	if (bestAxis < 0) {
		return false;
	}
	auto& objectBins = objectBinsForEachAxis[bestAxis];

	u32 leftCount, rightCount;
	AABB leftBounds, rightBounds;
	if (!spatialSplit) {
		_ASSERT(bestObjectSplit > 0);
		u32 axis = bestAxis;
		float k1 = BVH_SPLITS / size[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)
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
				if (bin < (int)bestObjectSplit && i < end - 1)
					i++;
				else
					break;
			}

			while (true)
			{
				int bin = static_cast<int>(k1 * (_centres[_secondaryIndexBuffer[j]][axis] - node->bounds.min[axis]));
				bin = glm::min(glm::max(0, bin), BVH_SPLITS - 1);
				if (bin >= (int)bestObjectSplit && j > 0)
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
		for (int bin = bestObjectSplit; bin < BVH_SPLITS; bin++)
		{
			rightCount += objectBins[bin].triangleCount;
			if (objectBins[bin].triangleCount > 0)
				rightBounds.fit(objectBins[bin].bounds);
		}

		for (int bin = 0; bin < (int)bestObjectSplit; bin++)
		{
			leftCount += objectBins[bin].triangleCount;
			if (objectBins[bin].triangleCount > 0)
				leftBounds.fit(objectBins[bin].bounds);
		}
	}
	else {
		u32 axis = bestAxis;
		float k1 = BVH_SPLITS / size[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)

		// allocate enough memory to hold the new faces of this node
		leftCount = 0;
		rightCount = 0;
		std::vector<u32> newTriangles;
		newTriangles.insert(newTriangles.end(), bestLeft.triangles.begin(), bestLeft.triangles.end());
		newTriangles.insert(newTriangles.end(), bestRight.triangles.begin(), bestRight.triangles.end());
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

void raytracer::SbvhBuilder::makeObjectBins(SubBvhNode* node, u32 axis, ObjectBin* bins)
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
		int bin = glm::min(glm::max(static_cast<int>(x), 0), BVH_SPLITS - 1);

		bins[bin].triangleCount++;
		bins[bin].bounds.fit(_aabbs[_secondaryIndexBuffer[i]]);
	}
}

void raytracer::SbvhBuilder::makeSpatialBins(SubBvhNode* node, u32 axis, SpatialSplit* spatialSplits)
{
	glm::vec3 size = node->bounds.max - node->bounds.min;
	u32 localFirstTriangleIndex = node->firstTriangleIndex;
	const u32 end = localFirstTriangleIndex + node->triangleCount;
	float k1 = BVH_SPLITS / size[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound
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
				AABB triangleBounds = clipTriangleBounds(axis, k1 * j + node->bounds.min[axis], k1 * (j + 1) + node->bounds.min[axis], _secondaryIndexBuffer[i]);
				SpatialSplitRef newRef;
				newRef.triangleIndex = _secondaryIndexBuffer[i];
				newRef.clippedBounds = triangleBounds;
				spatialSplits[j].refs.push_back(newRef);
				spatialSplits[j].bounds.fit(triangleBounds);
			}
		}
	}
}

bool raytracer::SbvhBuilder::doSingleObjectSplit(SubBvhNode* node, u32 axis, u32 split, float& outSah, ObjectBin* bins)
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
		surfaceAreaLeft = leftAABB.surfaceArea();
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
		surfaceAreaRight = rightAABB.surfaceArea();
	}

	outSah = triangleCountLeft * surfaceAreaLeft + triangleCountRight * surfaceAreaRight;
	return triangleCountLeft > 0 && triangleCountRight > 0;
}

bool raytracer::SbvhBuilder::doSingleSpatialSplit(SubBvhNode* node, u32 axis, u32 split, float& sah, FinalSplit& outLeft, FinalSplit& outRight, SpatialSplit* spatialSplits)
{
	std::unordered_set<u32> splitTriangles;

	FinalSplit left;
	// Calculate the triangle count and surface area of the AABB to the left of the possible split
	{
		for (int leftBin = 0; leftBin < split; leftBin++)
		{
			for (SpatialSplitRef& ref : spatialSplits[leftBin].refs) {
				left.triangles.insert(ref.triangleIndex);
			}
			if (spatialSplits[leftBin].refs.size() > 0)
				left.bounds.fit(spatialSplits[leftBin].bounds);
		}
	}

	FinalSplit right;
	// Calculate the triangle count and surface area of the AABB to the right of the possible split
	{
		for (int rightBin = split; rightBin < BVH_SPLITS; rightBin++)
		{
			for (SpatialSplitRef& ref : spatialSplits[rightBin].refs) {
				right.triangles.insert(ref.triangleIndex);
				if (left.triangles.find(ref.triangleIndex) != left.triangles.end()) {
					splitTriangles.insert(ref.triangleIndex);
				}
			}
			if (spatialSplits[rightBin].refs.size() > 0)
				right.bounds.fit(spatialSplits[rightBin].bounds);
		}
	}

	// unsplitting 
	for (u32 splittedId : splitTriangles) {
		float costSplit = right.bounds.surfaceArea() * right.triangles.size() + left.bounds.surfaceArea() * left.triangles.size();
		// we have to clip the unsplitted triangle again because if it has been split in a parent node it might be bigger than the node bounds.
		AABB triangleBounds = clipTriangleBounds(axis, node->bounds.min[axis], node->bounds.max[axis], _secondaryIndexBuffer[splittedId]);
		AABB leftBounds = left.bounds;
		left.bounds.fit(triangleBounds);
		AABB rightBounds = right.bounds;
		rightBounds.fit(triangleBounds);
		float costLeft = leftBounds.surfaceArea() * (left.triangles.size() - 1) + right.bounds.surfaceArea() * right.triangles.size();
		float costRight = rightBounds.surfaceArea() * (right.triangles.size() - 1) + left.bounds.surfaceArea() * left.triangles.size();

		if (costSplit < costLeft && costSplit < costRight) {
			continue;
		}
		else if (costLeft < costRight) {
			right.triangles.erase(right.triangles.find(splittedId));
			left.bounds = leftBounds;
		}
		else {
			left.triangles.erase(left.triangles.find(splittedId));
			right.bounds = rightBounds;
		}
	}

	sah = right.triangles.size() * right.bounds.surfaceArea() + left.triangles.size() * left.bounds.surfaceArea();

	outLeft = std::move(left);
	outRight = std::move(right);
	return left.triangles.size() > 0 && right.triangles.size() > 0;
}

raytracer::AABB raytracer::SbvhBuilder::clipTriangleBounds(u32 axis, float left, float right, u32 triangleId) {
	glm::vec3 clipped_vertices[9];
	u32 n_clipped_vertices = 0;

	glm::vec3 v[3]; // base triangle vertices
	v[0] = (glm::vec3) (*_vertices)[(*_triangles)[triangleId].indices[0]].vertex;
	v[1] = (glm::vec3) (*_vertices)[(*_triangles)[triangleId].indices[1]].vertex;
	v[2] = (glm::vec3) (*_vertices)[(*_triangles)[triangleId].indices[2]].vertex;

	for (auto& vertex : v) {
		if (vertex[axis] >= left && vertex[axis] < right) {
			// add the vertex to the temporary vertex buffer
			clipped_vertices[n_clipped_vertices] = vertex;
			n_clipped_vertices++;
		}
	}

	glm::vec3 e[3]; // base triangle edges
	e[0] = v[1] - v[0];
	e[1] = v[2] - v[1];
	e[2] = v[0] - v[2];

	// every segment is represented as v[i] + e[i] * t. We find t.
	for (u32 i = 0; i < 3; ++i) {
		// do line plane intersection
		glm::vec3 plane_center_min(0);
		glm::vec3 plane_center_max(0);
		glm::vec3 plane_normal(0);
		plane_center_min[axis] = left;
		plane_center_max[axis] = right;
		plane_normal[axis] = 1.f;

		float denom = glm::dot(plane_normal, e[i]);
		if (abs(denom) > 0.0001f)
		{
			float t;
			t = glm::dot(plane_center_min - v[i], plane_normal) / denom;
			if (t >= 0.f && t <= 1.f) {
				auto new_v = v[0] + e[0] * t;
				clipped_vertices[n_clipped_vertices] = new_v;
				++n_clipped_vertices;
			}
			t = glm::dot(plane_center_max - v[i], plane_normal) / denom;
			if (t >= 0.f && t <= 1.f) {
				auto new_v = v[0] + e[0] * t;
				clipped_vertices[n_clipped_vertices] = new_v;
				++n_clipped_vertices;
			}
		}
	}

	AABB triangleBounds;
	triangleBounds.min = glm::vec3(std::numeric_limits<float>::max());
	triangleBounds.max = glm::vec3(std::numeric_limits<float>::min());

	for (u32 i = 0; i < n_clipped_vertices; ++i) {
		triangleBounds.min = glm::min(triangleBounds.min, clipped_vertices[i]);
		triangleBounds.max = glm::max(triangleBounds.max, clipped_vertices[i]);
	}
	return triangleBounds;
}

