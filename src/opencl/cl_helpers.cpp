#include "cl_helpers.h"
#include <iostream>

cl_float3 glmToCl(glm::vec3 vec)
{
    return { vec.x, vec.y, vec.z };
}

void timeOpenCL(cl::Event& ev, std::string_view operationName)
{
#ifdef PROFILE_OPENCL
    ev.wait();
    cl_ulong startTime, stopTime;
    ev.getProfilingInfo(CL_PROFILING_COMMAND_START, &startTime);
    ev.getProfilingInfo(CL_PROFILING_COMMAND_END, &stopTime);
    double totalTime = stopTime - startTime;
    std::cout << "Timing (" << operationName << "): " << (totalTime / 1000000.0) << "ms" << std::endl;
#endif
}

void __checkClErr(cl_int errorCode, std::string_view file, int line, std::string_view message)
{
    if (errorCode != CL_SUCCESS) {

        std::cerr << "OpenCL ERROR: " << message << " " << errorCode << " (" << file << ":" << line << ")" << std::endl;
#ifdef _WIN32
        system("PAUSE");
#endif
        exit(EXIT_FAILURE);
    }
}
