#pragma once

#include "types.h"
#include <vector>
#include <set>
#include "aabb.h"
#include "vertices.h"

namespace raytracer {

	class Scene;
	struct SceneNode;
	struct SubBvhNode;

	class SbvhBuilder
	{
		struct ObjectBin {
			u32 triangleCount;
			AABB bounds;
			ObjectBin(u32 triangleCount = 0, AABB bounds = AABB()) : triangleCount(triangleCount), bounds(bounds) {}
		};

		struct SpatialSplitRef {
			u32 triangleIndex;
			AABB clippedBounds;
		};

		struct SpatialSplitBin {
			AABB bounds;
			std::vector<SpatialSplitRef> refs;
		};

		struct FinalSplit {
			AABB bounds;
			std::set<u32> triangles;
		};

	public:
		SbvhBuilder() { };
		~SbvhBuilder() { };

		u32 build(
			std::vector<VertexSceneData>& vertices,
			std::vector<TriangleSceneData>& triangles,
			std::vector<SubBvhNode>& outBvhNodes);// BVH may change this
	private:
		void subdivide(u32 nodeId);
		bool partition(u32 nodeId);
		AABB createBounds(u32 triangleIndex);
		u32 allocateNodePair();
		void makeObjectBins(u32 nodeId, u32 axis, ObjectBin* bins);
		void makeSpatialBins(u32 nodeId, u32 axis, SpatialSplitBin* bins);
		bool doSingleObjectSplit(u32 nodeId, u32 axis, u32 split, float& sah, ObjectBin* bins);
		bool doSingleSpatialSplit(u32 nodeId, u32 axis, u32 split, float& sah, FinalSplit& outLeft, FinalSplit& outRight, SpatialSplitBin* bins);
		raytracer::AABB clipTriangleBounds(u32 axis, float left, float right, u32 triangleId);
		bool checkNode(u32 nodeId);
	public:
		u32 _totalSplits;
		u32 _spatialSplits;
		u32 _totalNodes;
	private:
		// Used during binned BVH construction
		std::vector<TriangleSceneData>* _triangles;
		std::vector<VertexSceneData>* _vertices;
		std::vector<SubBvhNode>* _bvh_nodes;
		std::vector<std::vector<u32>> _node_triangle_list;
		std::vector<glm::vec3> _centres;
		std::vector<AABB> _aabbs;
	};
}