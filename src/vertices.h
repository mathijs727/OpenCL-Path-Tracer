#pragma once
#include "model/material.h"
#include "types.h"
#include <glm/glm.hpp>

namespace raytracer {
struct TriangleSceneData {
    glm::u32vec3 indices;
    uint32_t material_index;
};

struct VertexSceneData {
    glm::vec4 vertex;
    glm::vec4 normal;
    glm::vec2 texCoord;
    std::byte __padding[8];
};

struct EmissiveTriangle {
    glm::vec4 vertices[3];
    Material material;
};
}
