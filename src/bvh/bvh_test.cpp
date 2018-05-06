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
	const uint32_t max = maxTrianglesPerLeaf(_root_node);
	for (int i = 1; i <= 10; i++)
	{
		float stepLin = (float)max / i;
		float stepQuad = ((stepLin / max) * (stepLin / max)) * max;
		uint32_t stepMax = (uint32_t)stepQuad;
		uint32_t count = countTrianglesLessThen(_root_node, stepMax);

		std::cout << "< " << stepMax << ":\t\t" << count << std::endl;
	}

	std::cout << "\n\n" << std::endl;
}

uint32_t raytracer::BvhTester::countNodes(uint32_t nodeId)
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

uint32_t raytracer::BvhTester::countDepth(uint32_t nodeId)
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

uint32_t raytracer::BvhTester::countTriangles(uint32_t nodeId)
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

uint32_t raytracer::BvhTester::countLeafs(uint32_t nodeId)
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

uint32_t raytracer::BvhTester::maxTrianglesPerLeaf(uint32_t nodeId)
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

uint32_t raytracer::BvhTester::countTrianglesLessThen(uint32_t nodeId, uint32_t maxCount)
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

