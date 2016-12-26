#include "bvh_test.h"
#include <array>
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
	std::cout << "Number of bvh nodes visited: " << countNodes(_root_node) << " out of " << _bvhNodes.size() << std::endl;
	std::cout << "Recursion depth: " << countDepth(_root_node) << "\n" << std::endl;

	std::cout << "Number of triangles visited: " << countTriangles(_root_node) << " out of " << _triangles.size() << std::endl;
	std::cout << "Average triangle per leaf: " << (float)countTriangles(_root_node) / countLeafs(_root_node) << std::endl;
	std::cout << "Max triangles per leaf: " << maxTrianglesPerLeaf(_root_node) << "\n" << std::endl;

	// Histogram
	std::cout << "Triangle per leaf histogram:" << std::endl;
	const u32 max = maxTrianglesPerLeaf(_root_node);
	for (int i = 1; i <= 10; i++)
	{
		float stepLin = (float)max / i;
		float stepQuad = ((stepLin / max) * (stepLin / max)) * max;
		u32 stepMax = (u32)stepQuad;
		u32 count = countTrianglesLessThen(_root_node, stepMax);

		std::cout << "< " << stepMax << ":\t\t" << count << std::endl;
	}

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

u32 raytracer::BvhTester::countTriangles(u32 nodeId)
{
	auto& node = _bvhNodes[nodeId];
	if (node.triangleCount == 0)
	{
		return countTriangles(node.leftChildIndex) + countTriangles(node.leftChildIndex + 1);
	}
	else {
		return node.triangleCount;
	}
}

u32 raytracer::BvhTester::countLeafs(u32 nodeId)
{
	auto& node = _bvhNodes[nodeId];
	if (node.triangleCount == 0)
	{
		return countLeafs(node.leftChildIndex) + countLeafs(node.leftChildIndex + 1);
	}
	else {
		return 1;
	}
}

u32 raytracer::BvhTester::maxTrianglesPerLeaf(u32 nodeId)
{
	auto& node = _bvhNodes[nodeId];
	if (node.triangleCount == 0)
	{
		return std::max(maxTrianglesPerLeaf(node.leftChildIndex), maxTrianglesPerLeaf(node.leftChildIndex + 1));
	}
	else {
		return node.triangleCount;
	}
}

u32 raytracer::BvhTester::countTrianglesLessThen(u32 nodeId, u32 maxCount)
{
	auto& node = _bvhNodes[nodeId];
	if (node.triangleCount == 0)
	{
		return countTrianglesLessThen(node.leftChildIndex, maxCount) + \
			countTrianglesLessThen(node.leftChildIndex + 1, maxCount);
	}
	else {
		if (node.triangleCount < maxCount)
			return 1;
		else
			return 0;
	}
}

