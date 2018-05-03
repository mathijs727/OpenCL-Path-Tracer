#pragma once
#include "template/cl_gl_includes.h"
#include <glm/glm.hpp>
#include <cstdint>

typedef unsigned int uint;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int32_t i16;
typedef uint8_t u8;
typedef int8_t i8;
typedef unsigned char byte;

#define CL_VEC3(NAME) \
	union { \
		glm::vec3 NAME; \
		cl_float3 __cl_padding_ ## NAME; \
	}
#define UNION_STRUCT_FUNCTIONS(CLASSNAME) \
	CLASSNAME() { } \
	CLASSNAME(const CLASSNAME& other) { memcpy(this, &other, sizeof(CLASSNAME)); } \
	CLASSNAME& operator=(const CLASSNAME& other) { memcpy(this, &other, sizeof(CLASSNAME)); return *this; }