#pragma once
#include "opencl/cl_helpers.h"

union PixelRGBA {
    uint32_t value;
    struct
    {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };
};
