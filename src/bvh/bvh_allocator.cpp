#include "bvh_allocator.h"

namespace raytracer {

uint32_t BVHAllocator::allocatePair()
{
    uint32_t leftIndex = (uint32_t)m_nodes.size();
    m_nodes.emplace_back();
    m_nodes.emplace_back();
    return leftIndex;
}

const SubBvhNode& BVHAllocator::operator[](uint32_t i) const
{
    return m_nodes[i];
}

SubBvhNode& BVHAllocator::operator[](uint32_t i)
{
    return m_nodes[i];
}

std::vector<SubBvhNode>&& BVHAllocator::getNodesMove()
{
    return std::move(m_nodes);
}

}
