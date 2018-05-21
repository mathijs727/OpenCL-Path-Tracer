#pragma once
#include "opencl/cl_gl_includes.h"
#include "opencl/cl_helpers.h"
#include <glm/glm.hpp>

namespace raytracer {
struct AABB {
    CL_VEC3(min);
    CL_VEC3(max);

    AABB();
    AABB(glm::vec3 min, glm::vec3 max);
    AABB(const AABB&) = default;
    AABB& operator=(const AABB&) = default;

    void fit(glm::vec3 vec);
    void fit(const AABB& other);
    AABB operator+(const AABB& other) const;
    AABB intersection(const AABB& other) const;

    bool contains(glm::vec3 vec) const;
    bool partiallyContains(const AABB& other) const;
    bool fullyContains(const AABB& other) const;
    bool operator==(const AABB& other) const;

    glm::vec3 center() const;
    glm::vec3 extent() const;
    float surfaceArea() const;
};
}
