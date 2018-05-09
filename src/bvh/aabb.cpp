#include "aabb.h"

namespace raytracer {

AABB::AABB()
    : min(std::numeric_limits<float>::max())
    , max(std::numeric_limits<float>::lowest())
{
}

void AABB::fit(const AABB& other)
{
    glm::min(1, 2);
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
}

glm::vec3 AABB::size() const
{
    return max - min;
}

float AABB::surfaceArea() const
{
    glm::vec3 size = max - min;
    return 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
}

}
