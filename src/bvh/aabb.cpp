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

void AABB::extend(glm::vec3 vec)
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

glm::vec3 AABB::center() const
{
    return (min + max) / 2.0f;
}

glm::vec3 AABB::size() const
{
    return glm::max(glm::vec3(0.0f), max - min);
}

float AABB::surfaceArea() const
{
    glm::vec3 s = size();
    return 2.0f * (s.x * s.y + s.y * s.z + s.z * s.x);
}
}
