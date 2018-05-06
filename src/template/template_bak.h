// Template, major revision 6, update for INFOMOV
// IGAD/NHTV/UU - Jacco Bikker - 2006-2016

#pragma once

typedef unsigned int unsigned;
typedef unsigned char uchar;
typedef unsigned char byte;

#include <chrono>
#include "../game.h"

using namespace Tmpl8; // to use template classes
//using namespace std;				// to use stl vectors

inline float Rand(float range) { return ((float)rand() / RAND_MAX) * range; }
inline int IRand(int range) { return rand() % range; }
//int filesize(FILE* f);
void delay();

namespace Tmpl8 {

#define PI 3.14159265358979323846264338327950f
#define INVPI 0.31830988618379067153776752674503f

#ifdef WIN32
#define MALLOC64(x) _aligned_malloc(x, 64)
#define FREE64(x) _aligned_free(x)
#else
inline void* MALLOC64(size_t size)
{
    void* ptr;
    if (posix_memalign(&ptr, 64, size) != 0) {
        std::cout << "Aligned memory alloc failed!" << std::endl;
        exit(1);
    }
    return ptr;
}
//#define MALLOC64(x)			posix_memalign(64, x)
#define FREE64(x) free(x)
#endif

struct Timer {
    typedef std::chrono::high_resolution_clock Clock;
#ifdef __linux__
    typedef std::chrono::time_point<std::chrono::system_clock> TimePoint;
#else // Both OS X and Windows
    typedef std::chrono::steady_clock::time_point TimePoint;
#endif
    typedef std::chrono::microseconds MicroSeconds;

    TimePoint start;
    Timer()
        : start(get())
    {
        init();
    }
    float elapsed() const
    {
        auto diff = get() - start;
        auto durationMs = std::chrono::duration_cast<MicroSeconds>(
            diff);
        return static_cast<float>(durationMs.count()) / 1000000.0f;
    }
    static TimePoint get()
    {
        return Clock::now();
    }

    //static double to_time(const TimePoint vt) {
    //	// TODO...
    //}

    void reset() { start = get(); }
    static void init()
    {
    }
};

#define BADFLOAT(x) ((*(unsigned*)&x & 0x7f000000) == 0x7f000000)

}; // namespace Tmpl8
