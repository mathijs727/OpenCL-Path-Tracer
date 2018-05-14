#include "aabb.h"

namespace raytracer {

AABB::AABB()
    : min(std::numeric_limits<float>::max())
    , max(std::numeric_limits<float>::lowest())
{
}

AABB::AABB(glm::vec3 min, glm::vec3 max)
    : min(min)
    , max(max)
{
}

void AABB::fit(glm::vec3 vec)
{
    min = glm::min(min, vec);
    max = glm::max(max, vec);
}

void AABB::fit(const AABB& other)
{
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
}

AABB AABB::operator+(const AABB& other) const
{
    return { glm::min(min, other.min), glm::max(max, other.max) };
}

AABB AABB::intersection(const AABB& other) const
{
    return { glm::max(min, other.min), glm::min(max, other.max) };
}

bool AABB::contains(glm::vec3 vec) const
{
    return (vec[0] >= min[0] && vec[1] >= min[1] && vec[2] >= min[2]) && (vec[0] <= max[0] && vec[1] <= max[1] && vec[2] <= max[2]);
}

bool AABB::partiallyContains(const AABB& other) const
{
    glm::vec3 intersectMin = glm::max(min, other.min);
    glm::vec3 intersectMax = glm::min(max, other.max);
    glm::vec3 intersectExtent = intersectMax - intersectMin;
    return intersectExtent[0] >= 0.0f && intersectExtent[1] >= 0.0f && intersectExtent[2] >= 0.0f;
}

bool AABB::fullyContains(const AABB& other) const
{
    return contains(other.min) && contains(other.max);
}

bool AABB::operator==(const AABB& other) const
{
    return min == other.min && max == other.max;
}

glm::vec3 AABB::center() const
{
    return (min + max) / 2.0f;
}

glm::vec3 AABB::size() const
{
    return glm::max(glm::vec3(0.0f), max - min);
}

glm::vec3 AABB::extent() const
{
    return max - min;
}

float AABB::surfaceArea() const
{
    glm::vec3 s = size();
    return 2.0f * (s.x * s.y + s.y * s.z + s.z * s.x);
}
}
