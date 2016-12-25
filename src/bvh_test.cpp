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
	std::cout << "Number of triangles in model: " << _triangles.size() << std::endl;
	std::cout << "Number of bvh nodes in model: " << _bvhNodes.size() << std::endl;
	std::cout << "Number of bvh nodes visited: " << countNodes(_root_node) << std::endl;
	std::cout << "Recursion depth: " << countDepth(_root_node) << std::endl;
	std::cout << "\n\n" << std::endl;
}

u32 raytracer::BvhTester::countNodes(u32 nodeId)
{
	auto& node = _bvhNodes[nodeId];
	if (node.triangleCount == 0)
	{
		return countNodes(node.leftChildIndex) + countNodes(node.leftChildIndex + 1) + 1;
	}
	else {
		return 1;
	}
}

u32 raytracer::BvhTester::countDepth(u32 nodeId)
{
	auto& node = _bvhNodes[nodeId];
	if (node.triangleCount == 0)
	{
		return std::max(countDepth(node.leftChildIndex), countDepth(node.leftChildIndex + 1)) + 1;
	}
	else {
		return 1;
	}
}

