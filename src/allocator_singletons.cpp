#include "allocator_singletons.h"

using namespace raytracer;

LinearAllocator<VertexSceneData>& raytracer::getVertexAllocatorInstance()
{
	static auto instance = std::make_unique<LinearAllocator<VertexSceneData>>();
	return *instance.get();
}

LinearAllocator<TriangleSceneData>& raytracer::getTriangleAllocatorInstance()
{
	static auto instance = std::make_unique<LinearAllocator<TriangleSceneData>>();
	return *instance.get();
}

LinearAllocator<Material>& raytracer::getMaterialAllocatorInstance()
{
	static auto instance = std::make_unique<LinearAllocator<Material>>();
	return *instance.get();
}

LinearAllocator<SubBvhNode>& raytracer::getSubBvhAllocatorInstance()
{
	static auto instance = std::make_unique<LinearAllocator<SubBvhNode>>();
	return *instance.get();
}

LinearAllocator<TopBvhNode>& raytracer::getTopBvhAllocatorInstance()
{
	static auto instance = std::make_unique<LinearAllocator<TopBvhNode>>();
	return *instance.get();
}
