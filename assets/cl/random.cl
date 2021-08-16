#ifndef __RANDOM_CL
#define __RANDOM_CL

#ifdef RANDOM_XOR32
#define randStream uint
#define randHostStream uint
#define randCopyOverStreamsFromGlobal(N, LOC, GLOB) xor32CopyOverStreamsFromGlobal(N, LOC, GLOB)
#define randCopyOverStreamsToGlobal(N, GLOB, LOC) xor32CopyOverStreamsToGlobal(N, GLOB, LOC)
#define randRandomU01(STREAM) xor32RandomU01(STREAM)
#define randRandomInteger(STREAM, min, max) xor32RandomInteger(STREAM, min, max)
#else

#include "clRNG/clRNG.clh"
#include "clRNG/lfsr113.clh"
#ifdef RANDOM_LFSR113
#define randStream clrngLfsr113Stream
#define randHostStream clrngLfsr113HostStream
#define randCopyOverStreamsFromGlobal(N, LOC, GLOB) clrngLfsr113CopyOverStreamsFromGlobal(N, LOC, GLOB)
#define randCopyOverStreamsToGlobal(N, GLOB, LOC) clrngLfsr113CopyOverStreamsToGlobal(N, GLOB, LOC)
#define randRandomU01(STREAM) clrngLfsr113RandomU01(STREAM)
#define randRandomInteger(STREAM, min, max) clrngLfsr113RandomInteger(STREAM, min, max)
#endif

#endif


#ifdef RANDOM_XOR32
void xor32CopyOverStreamsFromGlobal(int n, uint* localStream, __global uint* globalStream)
{
	for (int i = 0; i < n; i++)
	{
		localStream[i] = globalStream[i];
	}
}


void xor32CopyOverStreamsToGlobal(int n, __global uint* globalStream, uint* localStream)
{
	for (int i = 0; i < n; i++)
	{
		globalStream[i] = localStream[i];
	}
}

float xor32RandomU01(uint* seed)
{
	*seed ^= *seed << 13;
	*seed ^= *seed >> 17;
	*seed ^= *seed << 5;
	return *seed * 2.3283064365387e-10f;
}

int xor32RandomInteger(uint* seed, int min, int max)
{
	*seed ^= *seed << 13;
	*seed ^= *seed >> 17;
	*seed ^= *seed << 5;
	int rand = as_int(*seed);
	return min + rand % (max - min + 1);
}
#endif

#endif // __RANDOM_CL