#ifndef KERNEL_DATA_CL
#define KERNEL_DATA_CL

typedef struct
{
	// Camera
	Camera camera;

	// Scene
	uint numEmissiveTriangles;
	uint topLevelBvhRoot;

	// Used for ray generation
	uint rayOffset;
	uint scrWidth;
	uint scrHeight;

	// Used for compaction
	uint numInRays;
	uint numOutRays;
	uint numShadowRays;
	uint maxRays;
	uint newRays;

	int cubemapTextureIndices[6];
} KernelData;

typedef struct
{
	float2 uv;
	const __global float* invTransform;
	int triangleIndex;
	float t;
	bool hit;
} ShadingData;

#endif // KERNEL_DATA_CL