#include "sbvh.h"
#include "allocator_singletons.h"
#include <numeric>
#include <array>



u32 raytracer::SbvhBuilder::build(u32 firstTriangle, u32 triangleCount)
{
	_centres.resize(triangleCount);
	_aabbs.resize(triangleCount);
	_secondaryIndexBuffer.resize(triangleCount);

	auto& vertices = getVertexAllocatorInstance();
	auto& faces = getTriangleAllocatorInstance();
	auto& bvhAllocator = getSubBvhAllocatorInstance();

	// initialise the secondary index buffer.
	// Secondary index buffer starts from 0 because it is also useful to index the _centres and _aabb buffers
	for (int i = 0; i < triangleCount; ++i) {
		_secondaryIndexBuffer[i] = i;
	}

	// Calculate centroids
	for (uint i = 0; i < triangleCount; ++i) {
		std::array<glm::vec3, 3> face;
		auto& triangle = faces[firstTriangle + i];
		face[0] = (glm::vec3) vertices[triangle.indices[0]].vertex;
		face[1] = (glm::vec3) vertices[triangle.indices[1]].vertex;
		face[2] = (glm::vec3) vertices[triangle.indices[2]].vertex;
		_centres[i] = std::accumulate(face.cbegin(), face.cend(), glm::vec3()) / 3.f;
	}

	// Calculate AABBs
	for (uint i = 0; i < triangleCount; ++i) {
		_aabbs[i] = createBounds(firstTriangle + i);
	}

}

void raytracer::SbvhBuilder::subdivide(u32 nodeId)
{

}

bool raytracer::SbvhBuilder::partition(u32 nodeId)
{

}

raytracer::AABB raytracer::SbvhBuilder::createBounds(u32 triangleIndex)
{

}

u32 raytracer::SbvhBuilder::allocateNodePair()
{

}
