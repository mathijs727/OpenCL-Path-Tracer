#pragma once
#include <glm.hpp>
#include <vector>
#include "types.h"
#include "template/includes.h"
#include "bvh_nodes.h"
#include "linear_allocator.h"
#include "vertices.h"

namespace raytracer {

	class Scene;
	struct SceneNode;

	class BinnedBvhBuilder
	{
	public:
		BinnedBvhBuilder() { };
		~BinnedBvhBuilder() { };

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

