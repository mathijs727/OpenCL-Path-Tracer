#include "template/cl_gl_includes.h"
#include <iostream>

#ifdef _DEBUG
#define checkClErr(ERROR_CODE, MESSAGE) __checkClErr(ERROR_CODE, __FILE__, __LINE__, MESSAGE)
#else
#define checkClErr(ERROR_CODE, MESSAGE)
#endif

// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
inline void __checkClErr(cl_int errorCode, std::string_view file, int line, std::string_view message)
{
    if (errorCode != CL_SUCCESS) {

        std::cerr << "OpenCL ERROR: " << message << " " << errorCode << " (" << file << ":" << line << ")" << std::endl;
#ifdef _WIN32
        system("PAUSE");
#endif
        exit(EXIT_FAILURE);
    }
}
