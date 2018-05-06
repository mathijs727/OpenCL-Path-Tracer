#pragma once
#include "aabb.h"

namespace raytracer {
struct TopBvhNode {
    AABB bounds;
    glm::mat4 invTransform;
    union {
        struct
        {
            uint32_t leftChildIndex;
            uint32_t rightChildIndex;
        };
        uint32_t subBvhNode;
    };
    uint32_t isLeaf;
    uint32_t __padding_cl; // Make the struct 16 byte aligned
};

struct SubBvhNode {
    AABB bounds;
    union {
        uint32_t leftChildIndex;
        uint32_t firstTriangleIndex;
    };
    uint32_t triangleCount;
    uint32_t __padding_cl[2];

    SubBvhNode()
    {
        leftChildIndex = 0;
        triangleCount = 0;
    }
};
}
