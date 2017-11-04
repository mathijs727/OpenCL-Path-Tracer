#pragma once
#include "types.h"
#include "model/material.h"
#include <glm/glm.hpp>

namespace raytracer
{
struct TriangleSceneData
{
	glm::u32vec3 indices;
	u32 material_index;
};

struct VertexSceneData
{
	glm::vec4 vertex;
	glm::vec4 normal;
	glm::vec2 texCoord;
	byte __padding[8];
};

struct EmissiveTriangle
{
	glm::vec4 vertices[3];
	Material material;
};
}