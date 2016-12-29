#include "sbvh.h"
#include <numeric>
#include <array>
#include <stack>
#include "bvh_nodes.h"
#include <xutility>
#include "aabb.h"
#include <glm/gtx/norm.hpp>

#define BVH_SPLITS 8
#define ALPHA 0.01f
#define COST_INTERSECTION 1.f
#define COST_TRAVERSAL 2.f
#define HI_QUALITY_CLIPS
//#define NO_UNSPLITTING

template<typename Itr>
bool checkRange(Itr begin, uint count) {
	std::set<Itr::value_type> test_set(begin, begin + count);
	return test_set.size() == count;
}

u32 raytracer::SbvhBuilder::build(
	std::vector<VertexSceneData>& vertices,
	std::vector<TriangleSceneData>& triangles,
	std::vector<SubBvhNode>& outBvhNodes)
{

	_totalSplits = 0;
	_spatialSplits = 0;

	u32 triangleCount = triangles.size();
	_triangles = &triangles;
	_vertices = &vertices;
	_bvh_nodes = &outBvhNodes;
	_centres.resize(triangleCount);
	_aabbs.resize(triangleCount);
	outBvhNodes.clear();

	std::vector<u32> startingTriangleList(triangleCount);
	for (u32 i = 0; i < triangleCount; ++i) {
		startingTriangleList[i] = i;
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
	_node_triangle_list[rootIndex] = std::move(startingTriangleList);
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
	bool result = checkNode(rootIndex);
	_ASSERT(result);
	// Subdivide the root node
	subdivide(rootIndex, 0);
	std::vector<TriangleSceneData> newFaces;
	std::stack<u32> nodesToVisit;
	nodesToVisit.push(rootIndex);
	while (!nodesToVisit.empty()) {
		u32 nodeId = nodesToVisit.top(); nodesToVisit.pop();
		auto& node = (*_bvh_nodes)[nodeId];
		auto& tris = _node_triangle_list[nodeId];
		if (node.triangleCount > 0) {
			node.firstTriangleIndex = newFaces.size();
			node.triangleCount = tris.size();
			for (u32 tri : tris) {
				newFaces.push_back((*_triangles)[tri]);
			}
		}
		else {
			nodesToVisit.push(node.leftChildIndex);
			nodesToVisit.push(node.leftChildIndex+1);
		}
	}
	*_triangles = std::move(newFaces);
	_totalNodes = _bvh_nodes->size() - 1;
	return rootIndex;
}

void raytracer::SbvhBuilder::subdivide(u32 nodeId, u32 level)
{
	if ((*_bvh_nodes)[nodeId].triangleCount <= 4)
		return;

	if (level > log2(_triangles->size())) return;

	if (partition(nodeId)) {// Divides our triangles over our children and calculates their bounds
		u32 leftIndex = (*_bvh_nodes)[nodeId].leftChildIndex;
		_ASSERT(leftIndex > nodeId);
		_ASSERT(checkNode(leftIndex));
		subdivide(leftIndex, level+1);
		_ASSERT(checkNode(leftIndex + 1));
		subdivide(leftIndex + 1, level+1);
	}
}

bool raytracer::SbvhBuilder::partition(u32 nodeId)
{
	// Use pointer because references are not reassignable (we need to reassign after allocation)
	auto* node = &(*_bvh_nodes)[nodeId];
	_ASSERT(checkNode(nodeId));
	// termination criterion. If no sah can be found higher than this it will not split
	float maxSah = (node->triangleCount - (float) COST_TRAVERSAL / COST_INTERSECTION) * node->bounds.surfaceArea();
	
	glm::vec3 size = node->bounds.max - node->bounds.min;
	u32 localFirstTriangleIndex = node->firstTriangleIndex;
	// Loop through the triangles and calculate bin dimensions and triangle count
	const u32 end = localFirstTriangleIndex + node->triangleCount;

	bool spatialSplit = false;
	float bestSah = maxSah;
	int bestAxis = -1;
	FinalSplit bestLeft, bestRight;
	std::array<ObjectBin, BVH_SPLITS> objectBinsForEachAxis[3];
	int bestObjectSplit = -1;
	for (u32 axis = 0; axis < 3; ++axis) {
		if (size[axis] == 0) continue;
		auto& localObjectBins = objectBinsForEachAxis[axis];
		makeObjectBins(nodeId, axis, localObjectBins.data());
		std::array<SpatialSplitBin, BVH_SPLITS> spatialBins;
		makeSpatialBins(nodeId, axis, spatialBins.data());

 		for (u32 split = 0; split < BVH_SPLITS; split++) {
			float objectSah;
			bool objectSplitGood = doSingleObjectSplit(nodeId, axis, split, objectSah, localObjectBins.data()) && (objectSah < bestSah);
			bool doSpatialSplit = true;
			if (objectSplitGood) {
				AABB leftBounds, rightBounds;
				for (u32 bin = 0; bin < split; ++bin)				 leftBounds.fit(localObjectBins[bin].bounds);
				for (u32 bin = split; bin < BVH_SPLITS; ++bin)		rightBounds.fit(localObjectBins[bin].bounds);
				AABB intersection;
				bool intersect = true;
				for (u32 ax = 0; ax < 3; ++ax) {
					if (leftBounds.min[axis] > rightBounds.max[axis] || rightBounds.min[axis] > leftBounds.max[axis]) {
						intersect = false;
						break;
					}
				}
				if (intersect) {
					for (u32 ax = 0; ax < 3; ++ax) {
						intersection.min[ax] = glm::max(leftBounds.min[ax], rightBounds.min[ax]);
						intersection.max[ax] = glm::min(leftBounds.max[ax], rightBounds.max[ax]);
					}
				}
				doSpatialSplit = intersect && ((intersection.surfaceArea() / node->bounds.surfaceArea()) > ALPHA);
			}
			FinalSplit left, right;
			float spatialSah;
			bool spatialSplitGood = doSpatialSplit && doSingleSpatialSplit(nodeId, axis, split, spatialSah, left, right, spatialBins.data()) && (spatialSah < bestSah);

			if (objectSplitGood && (!spatialSplitGood || objectSah < spatialSah)) {
				bestSah = objectSah;
				bestObjectSplit = split;
				spatialSplit = false;
				bestAxis = axis;
			}
			else if (spatialSplitGood) {
				bestSah = spatialSah;
				spatialSplit = true;
				bestLeft = std::move(left);
				bestRight = std::move(right);
				bestAxis = axis;
			}
		}
	}

	if (bestAxis < 0) {
		return false; // if no 
	}
	auto& objectBins = objectBinsForEachAxis[bestAxis];
	_totalSplits++;
	// apply the results of the selected split to the triangle buffer
	u32 leftCount, rightCount;
	AABB leftBounds, rightBounds;
	std::vector<u32> rightTriangles, leftTriangles;
	if (!spatialSplit) { // In case of regular object split
		_ASSERT(bestObjectSplit > 0);
		u32 axis = bestAxis; // for brevity
		float k1 = BVH_SPLITS / size[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)
		// Partition the array around the bin pivot
		// http://www.inf.fh-flensburg.de/lang/algorithmen/sortieren/quick/quicken.htm
		u32 i = localFirstTriangleIndex;
		u32 j = end - 1;
		
		for (u32 tri : _node_triangle_list[nodeId]) {
			auto centre = _centres[tri];
			int bin = static_cast<int>(k1 * (centre[axis] - node->bounds.min[axis]));
			bin = glm::clamp(bin, 0, BVH_SPLITS - 1);
			_ASSERT(bin >= 0 && bin < BVH_SPLITS);
			if (bin < (int)bestObjectSplit) {
				leftTriangles.push_back(tri);
			}
			else {
				rightTriangles.push_back(tri);
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

		for (int bin = 0; bin < bestObjectSplit; bin++)
		{
			leftCount += objectBins[bin].triangleCount;
			if (objectBins[bin].triangleCount > 0)
				leftBounds.fit(objectBins[bin].bounds);
		}
	}
	else { // in case of spatial split
		_spatialSplits++;
		u32 axis = bestAxis;
		leftCount = bestLeft.triangles.size();
		rightCount = bestRight.triangles.size();
		leftBounds = bestLeft.bounds;
		rightBounds = bestRight.bounds;
		_ASSERT(leftCount + rightCount >= node->triangleCount);
		leftTriangles.insert(leftTriangles.end(), bestLeft.triangles.begin(), bestLeft.triangles.end());
		rightTriangles.insert(rightTriangles.end(), bestRight.triangles.begin(), bestRight.triangles.end());
	}

	// Allocate child nodes
	u32 leftIndex = allocateNodePair();
	node = &(*_bvh_nodes)[nodeId];	// Allocation currently uses a std::vector, which means
									 // that addresses may change after allocation
	// Initialize child nodes
	auto& lNode = (*_bvh_nodes)[leftIndex];
	//lNode.firstTriangleIndex = node->firstTriangleIndex;
	lNode.triangleCount = leftCount;
	lNode.bounds = leftBounds;
	_node_triangle_list[leftIndex] = std::move(leftTriangles);
	_ASSERT(checkNode(leftIndex));
	_ASSERT(lNode.firstTriangleIndex >= 0);

	auto& rNode = (*_bvh_nodes)[leftIndex + 1];
	//rNode.firstTriangleIndex = node->firstTriangleIndex + leftCount;
	rNode.triangleCount = rightCount;
	rNode.bounds = rightBounds;
	_node_triangle_list[leftIndex + 1] = std::move(rightTriangles);
	_ASSERT(checkNode(leftIndex+1));
	_ASSERT(rNode.firstTriangleIndex >= 0);

	// set this node as not a leaf anymore
	node->triangleCount = 0;
	node->leftChildIndex = leftIndex;
	_node_triangle_list[nodeId].clear();
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

	for (u32 ax = 0; ax < 3; ++ax) {
		_ASSERT(min[ax] <= max[ax]);
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
	_node_triangle_list.emplace_back();
	_node_triangle_list.emplace_back();
	return index;
}

void raytracer::SbvhBuilder::makeObjectBins(u32 nodeId, u32 axis, ObjectBin* bins)
{
	auto node = &(*_bvh_nodes)[nodeId];
	for (int i = 0; i < BVH_SPLITS; i++)
		bins[i].triangleCount = 0;

	u32 localFirstTriangleIndex = node->firstTriangleIndex;
	glm::vec3 extents = node->bounds.max - node->bounds.min;
	_ASSERT(extents[axis] != 0);
	// Loop through the triangles and calculate bin dimensions and triangle count
	const u32 end = localFirstTriangleIndex + node->triangleCount;
	float k1 = BVH_SPLITS / extents[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound)
	for (u32 tri : _node_triangle_list[nodeId])
	{
		// Calculate the bin ID as described in the paper
		AABB bounds = _aabbs[tri];
		auto centre = _centres[tri];
		float x = k1 * (centre[axis] - node->bounds.min[axis]);
		int bin = glm::clamp(static_cast<int>(x), 0, BVH_SPLITS-1);
		//int bin = glm::min(glm::max(static_cast<int>(x), 0), BVH_SPLITS - 1);
		_ASSERT(bin >= 0 && bin < BVH_SPLITS);
		bins[bin].triangleCount++;
		bins[bin].bounds.fit(bounds);
	}
}

raytracer::AABB clipBounds(raytracer::AABB clipper, raytracer::AABB clippee) {
	auto result = clippee;
	for (u32 ax = 0; ax < 3; ++ax) {
		result.min[ax] = glm::max(clipper.min[ax], clippee.min[ax]);
		result.max[ax] = glm::min(clipper.max[ax], clippee.max[ax]);
	}
	return result;
}

void raytracer::SbvhBuilder::makeSpatialBins(u32 nodeId, u32 axis, SpatialSplitBin* bins)
{
	auto node = &(*_bvh_nodes)[nodeId];
	glm::vec3 size = node->bounds.max - node->bounds.min;
	float k1 = BVH_SPLITS / size[axis] * 0.999999f;// Prevents the bin out of bounds (if centroid on the right bound
	_ASSERT(size[axis] != 0);
	for (auto tri : _node_triangle_list[nodeId])
	{
		u32 triangleId = tri;
		// Calculate the bin ID for the minimum and maximum points
		AABB bounds = clipBounds(_aabbs[tri], node->bounds);
		float x = k1 * (bounds.min[axis] - node->bounds.min[axis]);
		int bin_min = glm::clamp(static_cast<int>(x), 0, BVH_SPLITS - 1);

		float y = k1 * (bounds.max[axis] - node->bounds.min[axis]);
		int bin_max = glm::clamp(static_cast<int>(y), 0, BVH_SPLITS - 1);

		// if contained within a single bin don't need clipping
		if (bin_min == bin_max) {
			AABB triangleBounds = clipBounds(bounds, node->bounds);
			SpatialSplitRef newRef;
			newRef.triangleIndex = triangleId;
			newRef.clippedBounds = triangleBounds;
			bins[bin_min].refs.push_back(newRef);
			bins[bin_min].bounds.fit(triangleBounds);
		}
		else {
			_ASSERT(bin_max > bin_min);
			// add the triangle to the required bins
			for (int j = bin_min; j <= bin_max; ++j) {
				//AABB triangleBounds = clipTriangleBounds(axis, j/k1 + node->bounds.min[axis], (j + 1)/k1 + node->bounds.min[axis], tri);
				AABB binBounds = node->bounds;
				binBounds.max[axis] = (j + 1) / k1 + node->bounds.min[axis];
				binBounds.min[axis] = (j) / k1 + node->bounds.min[axis];
#ifdef HI_QUALITY_CLIPS
				AABB triangleBounds = clipTriangleBounds(binBounds, tri);
#else
				AABB triangleBounds = clipBounds(binBounds, bounds);
#endif
				SpatialSplitRef newRef;
				newRef.triangleIndex = triangleId;
				newRef.clippedBounds = triangleBounds;
				bins[j].refs.push_back(newRef);
				bins[j].bounds.fit(triangleBounds);
			}
		}
	}
	u32 ntris = 0;
	for (u32 bin = 0; bin < BVH_SPLITS; ++bin) {
		ntris += bins[bin].refs.size();
	}
	_ASSERT(ntris >= node->triangleCount);
}

bool raytracer::SbvhBuilder::doSingleObjectSplit(u32 nodeId, u32 axis, u32 split, float& outSah, ObjectBin* bins)
{
	auto node = &(*_bvh_nodes)[nodeId];
	// Calculate the triangle count and surface area of the AABB to the left of the possible split
	int triangleCountLeft = 0;
	AABB leftAABB; 
	float surfaceAreaLeft = 0.0f;
	{
		for (u32 leftBin = 0; leftBin < split; leftBin++)
		{
			triangleCountLeft += bins[leftBin].triangleCount;
			if (bins[leftBin].triangleCount > 0)
				leftAABB.fit(bins[leftBin].bounds);
		}
		surfaceAreaLeft = leftAABB.surfaceArea();
	}

	// Calculate the triangle count and surface area of the AABB to the right of the possible split
	int triangleCountRight = 0;
	AABB rightAABB;
	float surfaceAreaRight = 0.0f;
	{		
		for (u32 rightBin = split; rightBin < BVH_SPLITS; rightBin++)
		{
			triangleCountRight += bins[rightBin].triangleCount;
			if (bins[rightBin].triangleCount > 0)
				rightAABB.fit(bins[rightBin].bounds);
		}
		surfaceAreaRight = rightAABB.surfaceArea();
	}

	outSah = triangleCountLeft * surfaceAreaLeft + triangleCountRight * surfaceAreaRight;
	return triangleCountLeft > 0
		&& triangleCountRight > 0
		&& glm::length(leftAABB.size()) > 0
		&& glm::length(rightAABB.size()) > 0;
}

bool raytracer::SbvhBuilder::doSingleSpatialSplit(u32 nodeId, u32 axis, u32 split, float& sah, FinalSplit& outLeft, FinalSplit& outRight, SpatialSplitBin* spatialSplits)
{
	auto node = &(*_bvh_nodes)[nodeId];
	FinalSplit left;
	for (u32 leftBin = 0; leftBin < split; leftBin++)
	{
		auto& bin = spatialSplits[leftBin];
		for (auto& ref : bin.refs) {
			left.triangles.insert(ref.triangleIndex);
		}
		if (bin.refs.size() > 0)
			left.bounds.fit(bin.bounds);
	}

	FinalSplit right;
	for (u32 rightBin = split; rightBin < BVH_SPLITS; rightBin++)
	{
		auto& bin = spatialSplits[rightBin];
		for (auto& ref : bin.refs) {
			right.triangles.insert(ref.triangleIndex);
		}
		if (bin.refs.size() > 0) {
			right.bounds.fit(bin.bounds);
		}
	}
	
	std::vector<u32> splitTriangles;
	for (u32 tri : left.triangles) {
		if (right.triangles.count(tri) > 0) {
			splitTriangles.push_back(tri);
		}
	}

	// unsplitting
	for (u32 tri : splitTriangles) {
#ifdef NO_UNSPLITTING
		continue;
#endif
		AABB triangleBounds = clipBounds(_aabbs[tri], node->bounds);
		AABB leftBoundsUnsplit = left.bounds;
		leftBoundsUnsplit.fit(triangleBounds);
		AABB rightBoundsUnsplit = right.bounds;
		rightBoundsUnsplit.fit(triangleBounds);
		float costSplit = right.bounds.surfaceArea() * right.triangles.size() + left.bounds.surfaceArea() * left.triangles.size();
		float costUnsplitLeft = leftBoundsUnsplit.surfaceArea() * (left.triangles.size() - 1) + right.bounds.surfaceArea() * right.triangles.size();
		float costUnsplitRight = rightBoundsUnsplit.surfaceArea() * (right.triangles.size() - 1) + left.bounds.surfaceArea() * left.triangles.size();
		if (costSplit < costUnsplitLeft && costSplit < costUnsplitRight) {
			continue;
		}
		else {
			if (costUnsplitLeft < costUnsplitRight) {
				right.triangles.erase(right.triangles.find(tri));
				left.bounds = leftBoundsUnsplit;
			}
			else {
				left.triangles.erase(left.triangles.find(tri));
				right.bounds = rightBoundsUnsplit;
			}
		}
	}

	for (u32 tri : _node_triangle_list[nodeId]) {
		_ASSERT((left.triangles.find(tri) != left.triangles.end()) || (right.triangles.find(tri) != right.triangles.end()));
	}

	
	_ASSERT(left.triangles.size() + right.triangles.size() >= node->triangleCount);
	sah = right.triangles.size() * right.bounds.surfaceArea() + left.triangles.size() * left.bounds.surfaceArea();
	outLeft = std::move(left);
	outRight = std::move(right);
	return outLeft.triangles.size() > 0
		&& outRight.triangles.size() > 0
		&& glm::length2(outLeft.bounds.size()) > 0
		&& glm::length2(outRight.bounds.size()) > 0;
}

raytracer::AABB raytracer::SbvhBuilder::clipTriangleBounds(u32 axis, float left, float right, u32 triangleId) {
	_ASSERT(right >= left);
	//
	//AABB bounds = _aabbs[triangleId];
	//bounds.min[axis] = glm::max(bounds.min[axis], left);
	//bounds.max[axis] = glm::min(bounds.max[axis], right);
	//return bounds;

	std::vector<glm::vec3> clipped_vertices;

	glm::vec3 v[3]; // base triangle vertices
	v[0] = (glm::vec3) (*_vertices)[(*_triangles)[triangleId].indices[0]].vertex;
	v[1] = (glm::vec3) (*_vertices)[(*_triangles)[triangleId].indices[1]].vertex;
	v[2] = (glm::vec3) (*_vertices)[(*_triangles)[triangleId].indices[2]].vertex;

	for (auto& vertex : v) {
		if (vertex[axis] >= left && vertex[axis] <= right) clipped_vertices.push_back(vertex);
	}

	glm::vec3 e[3]; // base triangle edges
	e[0] = v[1] - v[0];
	e[1] = v[2] - v[1];
	e[2] = v[0] - v[2];

	// every segment is represented as v[i] + e[i] * t. We find t.
	for (u32 i = 0; i < 3; ++i) {
		// do line plane intersection
		glm::vec3 plane_normal(0);
		plane_normal[axis] = 1.f;
		float mag = glm::length(e[i]);
		auto normal = e[i] / mag;
		float denom = glm::dot(plane_normal, normal);
		if (abs(denom) > 0.0001f)
		{
			float t;
			t = -(glm::dot(v[i], plane_normal) - left) / denom;
			if (t >= 0.f && t <= mag) {
				auto new_v = v[i] + normal * t;
				//_ASSERT(new_v[axis] >= left - 0.0001f && new_v[axis] <= right + 0.0001f);
				clipped_vertices.push_back(new_v);
			}
			t = -(glm::dot(v[i], plane_normal) - right) / denom;
			if (t >= 0.f && t <= mag) {
				auto new_v = v[i] + normal * t;
				//_ASSERT(new_v[axis] >= left - 0.0001f && new_v[axis] <= right + 0.0001f);
				clipped_vertices.push_back(new_v);
			}
		}
	}
	glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());// Work around bug in glm
	glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());// Work around bug in glm
	for (u32 i = 0; i < clipped_vertices.size(); ++i) {
		min = glm::min(min, clipped_vertices[i]);
		max = glm::max(max, clipped_vertices[i]);
	}
	AABB triangleBounds;
	triangleBounds.min = min;
	triangleBounds.max = max;
	return triangleBounds;
}


bool raytracer::SbvhBuilder::checkNode(u32 nodeId)
{
	auto node = &(*_bvh_nodes)[nodeId];
	auto begin = _node_triangle_list[nodeId].begin();
	return glm::length(node->bounds.size()) > 0; //&& checkRange(begin, node->triangleCount);
}

raytracer::AABB raytracer::SbvhBuilder::clipTriangleBounds(AABB bounds, u32 triangleId) {
	std::vector<glm::vec3> v(3); // base triangle vertices
	v[0] = (glm::vec3) (*_vertices)[(*_triangles)[triangleId].indices[0]].vertex;
	v[1] = (glm::vec3) (*_vertices)[(*_triangles)[triangleId].indices[1]].vertex;
	v[2] = (glm::vec3) (*_vertices)[(*_triangles)[triangleId].indices[2]].vertex;
	std::vector<glm::vec3> newV;
	// every segment is represented as v[i] + e[i] * t. We find t.
	for (u32 axis = 0; axis < 3; ++axis) for (u32 p = 0; p < 2; ++p) {
		bool minNotMax = p == 0;
		glm::vec3 plane_normal(0);
		plane_normal[axis] = 1.f;
		std::vector<u8> insideV(v.size());
		float left = bounds.min[axis];
		float right = bounds.max[axis];
		for (u32 i = 0; i < v.size(); ++i) {
			if (minNotMax) {
				insideV[i] = v[i][axis] >= left;
			}
			else {
				insideV[i] = v[i][axis] <= right;
			}
		}
		for (u32 i = 0; i < v.size(); ++i) {
			int prevI = (i == 0) ? (v.size() - 1) : (i - 1);
			if ((insideV[i] && !insideV[prevI]) || (insideV[prevI] && !insideV[i])) {
				auto e = v[i] - v[prevI];
				// do line plane intersection
				float mag = glm::length(e);
				auto normal = e / mag;
				float denom = glm::dot(plane_normal, normal);
				if (abs(denom) > 0)
				{
					float offset = minNotMax ? left : right;
					float t = -(v[i][axis] - offset) / denom;
					auto new_v = v[i] + normal * t;
					_ASSERT(abs(new_v[axis] - offset) < 0.0001f);
					newV.push_back(new_v);
				}
			}
			if (insideV[i]) {
				newV.push_back(v[i]);
			}
		}
		v = std::move(newV);
		newV.clear();
	}
	glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());// Work around bug in glm
	glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());// Work around bug in glm
	for (u32 i = 0; i < v.size(); ++i) for (u32 ax = 0; ax < 3; ++ax) {
		min[ax] = glm::min(min[ax], v[i][ax]);
		max[ax] = glm::max(max[ax], v[i][ax]);
	}
	AABB triangleBounds;
	triangleBounds.min = min;
	triangleBounds.max = max;
	return triangleBounds;
}