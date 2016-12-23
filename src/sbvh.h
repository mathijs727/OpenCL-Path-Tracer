#pragma once

#include "types.h"
#include <vector>
#include "aabb.h"

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

	public:
		SbvhBuilder() { };
		~SbvhBuilder() { };

		u32 build(u32 firstTriangle, u32 triangleCount);// BVH may change this
	private:
		void subdivide(u32 nodeId);
		bool partition(u32 nodeId);
		AABB createBounds(u32 triangleIndex);

		u32 allocateNodePair();
	private:
		// Used during binned BVH construction
		u32 _triangleOffset;
		std::vector<glm::vec3> _centres;
		std::vector<AABB> _aabbs;
		std::vector<u32> _secondaryIndexBuffer;
		bool doObjectSelection(SubBvhNode* node, u32 axis, u32& outBestSplit, float& outBestSah, ObjectBin* bins);
		bool doSpatialSelection(SubBvhNode* node, u32 axis, u32& outBestSplit, float& outBestSah, SpatialSplit* bins);
	};
}