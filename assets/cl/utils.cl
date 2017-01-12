#ifndef __UTILS_CL
#define __UTILS_CL

#ifndef APPLE
// These functions are defined on OSX already
float4 make_float4(float x, float y, float z, float w)
{
    float4 res;
    res.x = x;
    res.y = y;
    res.z = z;
    res.w = w;
    return res;
}
#endif

// Linearly interpolate between two values
float4 lerp(float4 a, float4 b, float w)
{
    return a + w*(b-a);
}

#endif // __UTILS_CL