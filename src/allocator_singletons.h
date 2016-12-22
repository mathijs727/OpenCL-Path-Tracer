#pragma once
#include "linear_allocator.h"
#include "vertices.h"
#include "material.h"
#include "bvh_nodes.h"

namespace raytracer
{
	LinearAllocator<VertexSceneData>& getVertexAllocatorInstance();
	LinearAllocator<TriangleSceneData>& getTriangleAllocatorInstance();
	LinearAllocator<Material>& getMaterialAllocatorInstance();
	LinearAllocator<SubBvhNode>& getSubBvhAllocatorInstance();
	LinearAllocator<TopBvhNode>& getTopBvhAllocatorInstance();

	void SwitchActiveBvhAllocator();
}