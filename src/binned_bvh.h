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

		u32 build(
			std::vector<VertexSceneData>& vertices,
			std::vector<TriangleSceneData>& triangles,
			std::vector<SubBvhNode>& outBvhNodes);// BVH may change this
	private:
		void subdivide(u32 nodeId);
		bool partition(u32 nodeId);
		AABB createBounds(const TriangleSceneData& triangle);

		u32 allocateNodePair();
	private:
		// Used during binned BVH construction
		std::vector<glm::vec3> _centres;
		std::vector<AABB> _aabbs;

		// Store triangle, vertex and bvh node vectors in the class during construction because
		//  passing everything using recursion is so ugly (large parameter lists)
		std::vector<TriangleSceneData>* _triangles;
		std::vector<VertexSceneData>* _vertices;
		std::vector<SubBvhNode>* _bvh_nodes;
	};
}
