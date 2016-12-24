#include "bvh_test.h"
#include <iostream>

using namespace raytracer;

void traverse(std::vector<SubBvhNode>& bvhNodes)
{
	
}

raytracer::BvhTester::BvhTester(std::shared_ptr<Mesh> mesh) :
	_vertices(mesh->getVertices()),
	_triangles(mesh->getTriangles()),
	_bvhNodes(mesh->getBvhNodes()),
	_root_node(mesh->getBvhRootNode())
{
}

raytracer::BvhTester::~BvhTester()
{
}

void raytracer::BvhTester::test()
{
	std::cout << "\n\n-----   BVH TEST STARTING   -----" << std::endl;
	//std::cout << "Number of triangles in model: " << _triangles.size() << std::endl;
	std::cout << "Number of bvh nodes in model: " << _bvhNodes.size() << std::endl;
	std::cout << "Number of bvh nodes visited: " << traverse(_root_node) << std::endl;
	std::cout << "\n\n" << std::endl;
}

u32 raytracer::BvhTester::traverse(u32 nodeId)
{
	auto& node = _bvhNodes[nodeId];
	if (node.triangleCount == 0)
	{
		std::cout << nodeId << " inner" << std::endl;
		return traverse(node.leftChildIndex) + traverse(node.leftChildIndex + 1) + 1;
	}
	else {
		std::cout << nodeId << " leaf" << std::endl;
		return 1;
	}
}
