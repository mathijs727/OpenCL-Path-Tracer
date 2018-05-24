#include "context.h"
#include "opencl/cl_gl_includes.h"
#include "opencl/cl_helpers.h"
#ifdef WIN32
#include <Windows.h>
#endif
#include <iostream>

static int setenv(std::string_view name, std::string_view value, int overwrite);

// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
// https://www.codeproject.com/articles/685281/opengl-opencl-interoperability-a-case-study-using
CLContext::CLContext()
{
#ifdef OPENCL_GL_INTEROP
    setenv("CUDA_CACHE_DISABLE", "1", 1);
    cl_int lError = CL_SUCCESS;
    std::string lBuffer;

    // Get platforms.
    cl_uint lNbPlatformId = 0;
    clGetPlatformIDs(0, nullptr, &lNbPlatformId);

    if (lNbPlatformId == 0) {
        std::cout << "Unable to find an OpenCL platform." << std::endl;
#ifdef _WIN32
        system("PAUSE");
#endif
        exit(EXIT_FAILURE);
    }

    // Loop on all platforms.
    std::vector<cl_platform_id> lPlatformIds(lNbPlatformId);
    clGetPlatformIDs(lNbPlatformId, lPlatformIds.data(), 0);

    // Try to find the device with the compatible context.
    cl_platform_id lPlatformId = 0;
    cl_device_id lDeviceId = 0;
    cl_context lContext = 0;

    for (size_t i = 0; i < lPlatformIds.size() && lContext == 0; ++i) {
        const cl_platform_id lPlatformIdToTry = lPlatformIds[i];

        // Get devices.
        cl_uint lNbDeviceId = 0;
        clGetDeviceIDs(lPlatformIdToTry, CL_DEVICE_TYPE_GPU, 0, 0, &lNbDeviceId);

        if (lNbDeviceId == 0) {
            continue;
        }

        std::vector<cl_device_id> lDeviceIds(lNbDeviceId);
        clGetDeviceIDs(lPlatformIdToTry, CL_DEVICE_TYPE_GPU, lNbDeviceId, lDeviceIds.data(), 0);

        // Create the properties for this context.
        cl_context_properties lContextProperties[] = {
        // We need to add information about the OpenGL context with
        // which we want to exchange information with the OpenCL context.
#if defined(WIN32)
            // We should first check for cl_khr_gl_sharing extension.
            CL_GL_CONTEXT_KHR,
            (cl_context_properties)wglGetCurrentContext(),
            CL_WGL_HDC_KHR,
            (cl_context_properties)wglGetCurrentDC(),
#elif defined(__linux__)
            // We should first check for cl_khr_gl_sharing extension.
            CL_GL_CONTEXT_KHR,
            (cl_context_properties)glXGetCurrentContext(),
            CL_GLX_DISPLAY_KHR,
            (cl_context_properties)glXGetCurrentDisplay(),
#elif defined(__APPLE__)
        // We should first check for cl_APPLE_gl_sharing extension.
#if 0
			// This doesn't work.
			CL_GL_CONTEXT_KHR , (cl_context_properties)CGLGetCurrentContext() ,
			CL_CGL_SHAREGROUP_KHR , (cl_context_properties)CGLGetShareGroup(CGLGetCurrentContext()) ,
#else
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
            (cl_context_properties)CGLGetShareGroup(CGLGetCurrentContext()),
#endif
#endif
            CL_CONTEXT_PLATFORM,
            (cl_context_properties)lPlatformIdToTry,
            0,
            0,
        };

        // Look for the compatible context.
        for (size_t j = 0; j < lDeviceIds.size(); ++j) {
            cl_device_id lDeviceIdToTry = lDeviceIds[j];
            cl_context lContextToTry = 0;

            lContextToTry = clCreateContext(
                lContextProperties,
                1, &lDeviceIdToTry,
                0, 0,
                &lError);
            if (lError == CL_SUCCESS) {
                // We found the context.
                lPlatformId = lPlatformIdToTry;
                lDeviceId = lDeviceIdToTry;
                lContext = lContextToTry;
                m_device = cl::Device(lDeviceId);
                m_context = cl::Context(lContext);
                break;
            }
        }
    }

    if (lDeviceId == 0) {
        std::cout << "Unable to find a compatible OpenCL device." << std::endl;
#ifdef _WIN32
        system("PAUSE");
#endif
        exit(EXIT_FAILURE);
    }
#else
    // Let the user select a platform
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    std::cout << "Platforms:" << std::endl;
    for (int i = 0; i < (int)platforms.size(); i++) {
        std::string platformName;
        platforms[i].getInfo(CL_PLATFORM_NAME, &platformName);
        std::cout << "[" << i << "] " << platformName << std::endl;
    }
    cl::Platform platform;
    {
        int platformIndex;
        std::cout << "Select a platform: ";
        std::cin >> platformIndex;
        //platformIndex = 0;
        platform = platforms[platformIndex];
    }

    // Let the user select a device
    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
    std::cout << "\nDevices:" << std::endl;
    for (int i = 0; i < (int)devices.size(); i++) {
        std::string deviceName;
        devices[i].getInfo(CL_DEVICE_NAME, &deviceName);
        std::cout << "[" << i << "] " << deviceName << std::endl;
    }
    {
        int deviceIndex;
        std::cout << "Select a device: ";
        std::cin >> deviceIndex;
        //deviceIndex = 0;
        m_device = devices[deviceIndex];
    }

    // Create OpenCL context
    cl_int lError;
    m_context = cl::Context(devices, NULL, NULL, NULL, &lError);
    checkClErr(lError, "cl::Context");
#endif

    std::string openCLVersion;
    m_device.getInfo(CL_DEVICE_VERSION, &openCLVersion);
    std::cout << "OpenCL version: " << openCLVersion << std::endl;

    // Create a command queue.
#ifdef PROFILE_OPENCL
    cl_command_queue_properties props = CL_QUEUE_PROFILING_ENABLE;
#else
    cl_command_queue_properties props = 0;
#endif
    checkClErr(lError, "Unable to create an OpenCL command queue.");
    m_graphicsQueue = cl::CommandQueue(m_context, m_device, props, &lError);
    checkClErr(lError, "Unable to create an OpenCL command queue.");
    m_copyQueue = cl::CommandQueue(m_context, m_device, props, &lError);
    checkClErr(lError, "Unable to create an OpenCL command queue.");
}

cl::Context CLContext::getContext() const
{
    return m_context;
}

CLContext::operator cl::Context() const
{
    return m_context;
}

cl::Device CLContext::getDevice() const
{
    return m_device;
}

cl::CommandQueue CLContext::getGraphicsQueue() const
{
    return m_graphicsQueue;
}

cl::CommandQueue CLContext::getCopyQueue() const
{
    return m_copyQueue;
}

static int setenv(std::string_view name, std::string_view value, int overwrite)
{
#ifdef WIN32
    int errcode = 0;
    if (!overwrite) {
        size_t envsize = 0;
        errcode = getenv_s(&envsize, NULL, 0, name.data());
        if (errcode || envsize)
            return errcode;
    }
    return _putenv_s(name.data(), value.data());
#else
    return 0;
#endif
}
