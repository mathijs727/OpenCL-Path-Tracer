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
    AABB(const AABB&) = default;
    AABB& operator=(const AABB&) = default;

    void fit(const AABB& other);

    glm::vec3 size() const;
    float surfaceArea() const;
};
}
