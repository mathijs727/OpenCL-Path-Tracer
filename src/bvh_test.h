#pragma once
#include <vector>
#include "bvh_nodes.h"
#include "vertices.h"
#include "types.h"
#include "mesh.h"

namespace raytracer
{
	class BvhTester
	{
	public:
		BvhTester(std::shared_ptr<Mesh> mesh);
		~BvhTester();

		void test();
	private:
		u32 traverse(u32 node);
	private:
		u32 _root_node;
		const std::vector<VertexSceneData>& _vertices;
		const std::vector<TriangleSceneData>& _triangles;
		const std::vector<SubBvhNode>& _bvhNodes;
	};
}