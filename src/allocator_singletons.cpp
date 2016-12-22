#include "allocator_singletons.h"

using namespace raytracer;

uint s_active_bvh_allocator = 0;

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
	static std::unique_ptr<LinearAllocator<TopBvhNode>> instances[2] = {
		std::make_unique<LinearAllocator<TopBvhNode>>(),
		std::make_unique<LinearAllocator<TopBvhNode>>()
	};
	return *(instances[s_active_bvh_allocator]).get();
}

void raytracer::SwitchActiveBvhAllocator()
{
	s_active_bvh_allocator = (s_active_bvh_allocator + 1) % 2;
}
