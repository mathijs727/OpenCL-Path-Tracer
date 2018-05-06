#pragma once
#include <vector>
#include "bvh_nodes.h"
#include "vertices.h"
#include "types.h"
#include "model/mesh.h"

namespace raytracer
{
	class BvhTester
	{
	public:
		BvhTester(std::shared_ptr<Mesh> mesh);
		~BvhTester();

		void test();
	private:
		uint32_t countNodes(uint32_t nodeId);
		uint32_t countDepth(uint32_t nodeId);
		uint32_t countTriangles(uint32_t nodeId);
		uint32_t countLeafs(uint32_t nodeId);
		uint32_t maxTrianglesPerLeaf(uint32_t nodeId);
		uint32_t countTrianglesLessThen(uint32_t nodeId, uint32_t maxCount);
	private:
		uint32_t _root_node;
		const std::vector<VertexSceneData>& _vertices;
		const std::vector<TriangleSceneData>& _triangles;
		const std::vector<SubBvhNode>& _bvhNodes;
	};
}