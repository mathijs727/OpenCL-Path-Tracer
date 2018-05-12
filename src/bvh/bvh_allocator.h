#pragma once
#include "bvh_nodes.h"
#include <cstdint>
#include <vector>

namespace raytracer {
class BVHAllocator {
public:
    uint32_t allocatePair();

    const SubBvhNode& operator[](uint32_t i) const;
    SubBvhNode& operator[](uint32_t i);

    std::vector<SubBvhNode>&& getNodesMove();

private:
    std::vector<SubBvhNode> m_nodes;
};
}
