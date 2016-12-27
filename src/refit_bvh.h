#pragma once
#include <glm.hpp>
#include <vector>
#include "types.h"
#include "template/includes.h"
#include "bvh_nodes.h"
#include "vertices.h"
#include "aabb.h"

namespace raytracer {

	class Scene;
	struct SceneNode;

	class RefittingBvhBuilder
	{
	public:
		RefittingBvhBuilder() { };
		~RefittingBvhBuilder() { };

		void update(
			const std::vector<VertexSceneData>& vertices,
			const std::vector<TriangleSceneData>& triangles,
			std::vector<SubBvhNode>& outBvhNodes);// BVH may change this
	private:
		AABB createBounds(const TriangleSceneData& triangle, const std::vector<VertexSceneData>& vertices);
	private:
		// Used during binned BVH construction
		std::vector<AABB> _aabbs;
	};
}

