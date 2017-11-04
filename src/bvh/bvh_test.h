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
		u32 countNodes(u32 nodeId);
		u32 countDepth(u32 nodeId);
		u32 countTriangles(u32 nodeId);
		u32 countLeafs(u32 nodeId);
		u32 maxTrianglesPerLeaf(u32 nodeId);
		u32 countTrianglesLessThen(u32 nodeId, u32 maxCount);
	private:
		u32 _root_node;
		const std::vector<VertexSceneData>& _vertices;
		const std::vector<TriangleSceneData>& _triangles;
		const std::vector<SubBvhNode>& _bvhNodes;
	};
}