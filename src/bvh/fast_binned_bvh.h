#pragma once
#include <vector>
#include "types.h"
#include "bvh_nodes.h"
#include "aabb.h"
#include "vertices.h"

namespace raytracer {

	class Scene;
	struct SceneNode;

	class FastBinnedBvhBuilder
	{
	public:
		FastBinnedBvhBuilder() { };
		~FastBinnedBvhBuilder() { };

		uint32_t build(
			std::vector<VertexSceneData>& vertices,
			std::vector<TriangleSceneData>& triangles,
			std::vector<SubBvhNode>& outBvhNodes);// BVH may change this
	private:
		void subdivide(uint32_t nodeId);
		bool partition(uint32_t nodeId);
		AABB createBounds(const TriangleSceneData& triangle);

		uint32_t allocateNodePair();
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
