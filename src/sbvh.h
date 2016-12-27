#pragma once

#include "types.h"
#include <vector>
#include <unordered_set>
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

		struct SpatialSplit {
			AABB bounds;
			std::vector<SpatialSplitRef> refs;
		};

		struct FinalSplit {
			AABB bounds;
			std::unordered_set<u32> triangles;
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
	private:
		// Used during binned BVH construction
		std::vector<TriangleSceneData>* _triangles;
		std::vector<VertexSceneData>* _vertices;
		std::vector<SubBvhNode>* _bvh_nodes;
		u32 _triangleOffset;
		std::vector<glm::vec3> _centres;
		std::vector<AABB> _aabbs;
		std::vector<u32> _secondaryIndexBuffer;
		bool doObjectSelection(SubBvhNode* node, u32 axis, u32& outBestSplit, float& outBestSah, ObjectBin* bins);
		raytracer::AABB clipTriangleBounds(u32 axis, float left, float right, u32 triangleId);
		bool doSpatialSelection(SubBvhNode* node, u32 axis, FinalSplit& outLeft, FinalSplit& outRight, float& outBestSah);
	};
}