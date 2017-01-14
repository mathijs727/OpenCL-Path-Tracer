#ifndef __ATOMIC_CL
#define __ATOMIC_CL

#ifdef __GPU__// Ifdef AMD GPU which has 64 thread wavefront so we dont need memfence assuming workgroup size is 64
#define barrier64(type) barrier(type)
#else
#define barrier64(type) barrier(type)
#endif

uint workgroup_counter_inc(__global volatile uint* counter, bool active)
{
	size_t lid = get_local_id(0);
	__local volatile uint localCounter;
	__local uint globalOffset;
	
	if (lid == 0)
		localCounter = 0;

	barrier(CLK_LOCAL_MEM_FENCE);

	uint localOffset = 0;
	if (active)
		localOffset = atomic_inc(&localCounter);

	barrier(CLK_LOCAL_MEM_FENCE);

	if (lid == 0)
		globalOffset = atomic_add(counter, localCounter);

 	barrier(CLK_GLOBAL_MEM_FENCE);

	return globalOffset + localOffset;



	/*int offset = 1;
	__local uint data[64];
	
	data[lid] = active;

	for (int d = 64/2; d > 0; d /= 2)
	{
		barrier64(CLK_LOCAL_MEM_FENCE);
		if (lid < d)
		{
			int ai = offset * (2 * lid + 1) - 1;
			int bi = offset * (2 * lid + 2) - 1;
			data[bi] += data[ai];
		}
		offset *= 2;
	}

	barrier64(CLK_LOCAL_MEM_FENCE);
	if (lid == 0)
		data[63] = 0;
	barrier64(CLK_LOCAL_MEM_FENCE);

	for (int d = 1; d < 64; d *= 2)
	{
		offset /= 2;
		barrier64(CLK_LOCAL_MEM_FENCE);
		if (lid < d)
		{
			int ai = offset * (2 * lid + 1) - 1;
			int bi = offset * (2 * lid + 2) - 1;
			uint t = data[ai];
			data[ai] = data[bi];
			data[bi] += t;
		}
	}

	barrier64(CLK_LOCAL_MEM_FENCE);

	uint localOffset = data[lid];

	uint globalOffset;
	if (lid == 63)
	{
		uint localCount = localOffset + active;
		globalOffset = atomic_add(counter, localCount);
	}

	barrier64(CLK_GLOBAL_MEM_FENCE);

	return globalOffset + localOffset;*/
}

/*uint workgroup_counter_inc(__global volatile uint* counter, bool active)
{
	size_t lid = get_local_id(0);
	__local volatile uint localCounter;
	__local uint globalOffset;
	
	if (lid == 0)
		localCounter = 0;

	barrier64(CLK_LOCAL_MEM_FENCE);

	uint localOffset = 0;
	if (active)
		localOffset = atomic_inc(&localCounter);

	barrier64(CLK_LOCAL_MEM_FENCE);

	if (lid == 0)
		globalOffset = atomic_add(counter, localCounter);

 	barrier64(CLK_GLOBAL_MEM_FENCE);

	return globalOffset + localOffset;
}*/

#endif // __ATOMIC_CL