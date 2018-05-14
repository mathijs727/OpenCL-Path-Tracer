#pragma once
#include "opencl/cl_gl_includes.h"
#include "types.h"
#include <glm/glm.hpp>

namespace raytracer {
struct AABB {
    union {
        glm::vec3 min;
        cl_float3 __min_cl;
    };
    union {
        glm::vec3 max;
        cl_float3 __max_cl;
    };

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
    glm::vec3 size() const;
    glm::vec3 extent() const;
    float surfaceArea() const;
};
}
