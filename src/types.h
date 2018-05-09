#pragma once
#include "opencl/cl_gl_includes.h"
#include <glm/glm.hpp>

#define CL_VEC3(NAME)                  \
    union {                            \
        glm::vec3 NAME;                \
        cl_float3 __cl_padding_##NAME; \
    }
