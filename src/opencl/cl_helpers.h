#include "opencl/cl_gl_includes.h"
#include <glm/glm.hpp>
#include <string_view>

#define CL_VEC3(NAME)                  \
    union {                            \
        glm::vec3 NAME;                \
        cl_float3 __cl_padding_##NAME; \
    }

#ifdef _DEBUG
#define checkClErr(ERROR_CODE, MESSAGE) __checkClErr(ERROR_CODE, __FILE__, __LINE__, MESSAGE)
#else
#define checkClErr(ERROR_CODE, MESSAGE)
#endif

cl_float3 glmToCl(glm::vec3 vec);
void timeOpenCL(cl::Event& ev, std::string_view operationName);

// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
void __checkClErr(cl_int errorCode, std::string_view file, int line, std::string_view message);
