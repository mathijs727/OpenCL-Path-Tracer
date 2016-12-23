#pragma once

#include "types.h"
#include <vector>
#include "aabb.h"

namespace raytracer {

	class Scene;
	struct SceneNode;

	class SbvhBuilder
	{
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
	};
}