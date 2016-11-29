#define GLEW_STATIC
#define NOMINMAX
#ifdef _WIN32
#include <OpenGL/glew.h>// Header files included in project
#include <GL/wglew.h>
#else
#include <GL/glew.h>// Provided by system
#endif
#include "cl.hpp"

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