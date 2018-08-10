#pragma once
#include "cl12.hpp"
#include <GL/glew.h>
// Prevent clang-format from reordering includes: glew should always be included before opengl
#include <GL/gl.h>
#ifndef _WIN32
#include <GL/glx.h>
#endif

//#define PROFILE_OPENCL 1
#define OPENCL_GL_INTEROP 1
