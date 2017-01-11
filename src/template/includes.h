#define GLEW_STATIC
#define NOMINMAX
#ifdef _WIN32
#include <OpenGL/glew.h>// Header files included in project
#include <GL/wglew.h>
#else
#include <GL/glew.h>// Provided by system
#endif

#define CL_HPP_MINIMUM_OPENCL_VERSION 200
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include "cl12.hpp"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#ifdef __APPLE__
// Find CGLGetCurrentContext on macOS
#include <OpenGL/OpenGL.h>
#include <OpenCL/cl_gl_ext.h>
#endif