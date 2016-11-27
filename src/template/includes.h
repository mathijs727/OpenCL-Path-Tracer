#define GLEW_STATIC
#ifdef __WIN32
#include <OpenGL/glew.h>// Header files included in project
#else
#include <GL/glew.h>// Provided by system
#endif
#include "cl.hpp"

#ifdef __APPLE__
// Find CGLGetCurrentContext on macOS
#include <OpenGL/OpenGL.h>
#include <OpenCL/cl_gl_ext.h>
#endif