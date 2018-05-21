#pragma once
#include "bvh_nodes.h"
#include <cstdint>
#include <vector>

namespace raytracer {
class BVHAllocator {
public:
    uint32_t allocatePair();

    const SubBVHNode& operator[](uint32_t i) const;
    SubBVHNode& operator[](uint32_t i);

    std::vector<SubBVHNode>&& getNodesMove();

private:
    std::vector<SubBVHNode> m_nodes;
};
}
