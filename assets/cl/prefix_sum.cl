// Source:
// https://github.com/boxerab/clpp/blob/master/src/clpp/clppScan_GPU.cl

//------------------------------------------------------------
// Purpose :
// ---------
// Prefix sum or prefix scan is an operation where each output element contains the sum of all input elements preceding it.
//
// Algorithm :
// -----------
// The parallel prefix sum has two principal parts, the reduce phase (also known as the up-sweep phase) and the down-sweep phase.
//
// In the up-sweep reduction phase we traverse the computation tree from bottom to top, computing partial sums.
// After this phase, the last element of the array contains the total sum.
//
// During the down-sweep phase, we traverse the tree from the root and use the partial sums to build the scan in place.
//
// Because the scan pictured is an exclusive sum, a zero is inserted into the last element before the start of the down-sweep phase.
// This zero is then propagated back to the first element.
//
// In our implementation, each compute unit loads and sums up two elements (for the deepest depth). Each subsequent depth during the up-sweep
// phase is processed by half of the compute units from the deeper level and the other way around for the down-sweep phase.
//
// In order to be able to scan large arrays, i.e. arrays that have many more elements than the maximum size of a work-group, the prefix sum has to be decomposed.
// Each work-group computes the prefix scan of its sub-range and outputs a single number representing the sum of all elements in its sub-range.
// The workgroup sums are scanned using exactly the same algorithm.
// When the number of work-group results reaches the size of a work-group, the process is reversed and the work-group sums are
// propagated to the sub-ranges, where each work-group adds the incoming sum to all its elements, thus producing the final scanned array.
//
// References :
// ------------
// http://graphics.idav.ucdavis.edu/publications/print_pub?pub_id=1041
//
// To read :
// http://developer.apple.com/library/mac/#samplecode/OpenCL_Parallel_Prefix_Sum_Example/Listings/scan_kernel_cl.html#//apple_ref/doc/uid/DTS40008183-scan_kernel_cl-DontLinkElementID_5
// http://developer.apple.com/library/mac/#samplecode/OpenCL_Parallel_Reduction_Example/Listings/reduce_int4_kernel_cl.html
//------------------------------------------------------------

#pragma OPENCL EXTENSION cl_amd_printf : enable

#define T uint
#define OPERATOR_INDEXOF(I) I
#define OPERATOR_APPLY(A,B) A+B
#define OPERATOR_IDENTITY 0

//#define VOLATILE volatile
#define VOLATILE

//------------------------------------------------------------
// kernel__scanInter
//
// Purpose : do a scan on a chunck of data.
//------------------------------------------------------------

inline T scan_simt_exclusive(__local VOLATILE T* input, size_t idx, const uint lane)
{
	if (lane > 0 ) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 1)] , input[OPERATOR_INDEXOF(idx)]);
	if (lane > 1 ) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 2)] , input[OPERATOR_INDEXOF(idx)]);
	if (lane > 3 ) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 4)] , input[OPERATOR_INDEXOF(idx)]);
	if (lane > 7 ) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 8)] , input[OPERATOR_INDEXOF(idx)]);
	if (lane > 15) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 16)], input[OPERATOR_INDEXOF(idx)]);
		
	return (lane > 0) ? input[idx-1] : OPERATOR_IDENTITY;
}

inline T scan_simt_inclusive(__local VOLATILE T* input, size_t idx, const uint lane)
{	
	if (lane > 0 ) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 1)] , input[OPERATOR_INDEXOF(idx)]);
	if (lane > 1 ) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 2)] , input[OPERATOR_INDEXOF(idx)]);
	if (lane > 3 ) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 4)] , input[OPERATOR_INDEXOF(idx)]);
	if (lane > 7 ) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 8)] , input[OPERATOR_INDEXOF(idx)]);
	if (lane > 15) input[idx] = OPERATOR_APPLY(input[OPERATOR_INDEXOF(idx - 16)], input[OPERATOR_INDEXOF(idx)]);
		
	return input[idx];
}

inline T scan_workgroup_exclusive(__local T* localBuf, const uint idx, const uint lane, const uint simt_bid)
{
	// Step 1: Intra-warp scan in each warp
	T val = scan_simt_exclusive(localBuf, idx, lane);
	barrier(CLK_LOCAL_MEM_FENCE);
	
	// Step 2: Collect per-warp partial results (the sum)
	if (lane > 30) localBuf[simt_bid] = localBuf[idx];
	barrier(CLK_LOCAL_MEM_FENCE);
	
	// Step 3: Use 1st warp to scan per-warp results
	if (simt_bid < 1) scan_simt_inclusive(localBuf, idx, lane);
	barrier(CLK_LOCAL_MEM_FENCE);
	
	// Step 4: Accumulate results from Steps 1 and 3
	if (simt_bid > 0) val = OPERATOR_APPLY(localBuf[simt_bid-1], val);
	barrier(CLK_LOCAL_MEM_FENCE);
	
	// Step 5: Write and return the final result
	localBuf[idx] = val;
	barrier(CLK_LOCAL_MEM_FENCE);
	
	return val;
}

__kernel
void kernel__scan_block_anylength(
	__local T* localBuf,
	__global T* dataSet,
	const uint B,
	uint size,
	const uint passesCount
)
{	
	size_t idx = get_local_id(0);
	const uint bidx = get_group_id(0);
	const uint TC = get_local_size(0);
	
	const uint lane = idx & 31;
	const uint simt_bid = idx >> 5;
	
	T reduceValue = OPERATOR_IDENTITY;
	
	//#pragma unroll 4
	for(uint i = 0; i < passesCount; ++i)
	{
		const uint offset = i * TC + (bidx * B);
		const uint offsetIdx = offset + idx;
		
#ifdef OCL_PLATFORM_AMD
		if (offsetIdx > size-1)
		{
			// To avoid to lock !
			barrier(CLK_LOCAL_MEM_FENCE);
			barrier(CLK_LOCAL_MEM_FENCE);
			barrier(CLK_LOCAL_MEM_FENCE);
			
			barrier(CLK_LOCAL_MEM_FENCE);
			barrier(CLK_LOCAL_MEM_FENCE);
			barrier(CLK_LOCAL_MEM_FENCE);
			barrier(CLK_LOCAL_MEM_FENCE);
			barrier(CLK_LOCAL_MEM_FENCE);
			continue;
		}
#else
		if (offsetIdx > size-1) return;
#endif

		// Step 1: Read TC elements from global (off-chip) memory to local memory (on-chip)
		T input = localBuf[idx] = dataSet[offsetIdx];		
		
		/*
		// This version try to avoid bank conflicts and improve memory access serializations !
		if (lane < 1)
		{
			__global T* currentOffset = inputDatas + offsetIdx;
			vstore16(vload16(0, currentOffset),  0, localBuf);
			vstore16(vload16(0, currentOffset + 16), 16, localBuf);
		}
		barrier(CLK_LOCAL_MEM_FENCE);
		T input = localBuf[idx];
		*/
		
		barrier(CLK_LOCAL_MEM_FENCE);
		
		// Step 2: Perform scan on TC elements
		T val = scan_workgroup_exclusive(localBuf, idx, lane, simt_bid);
		
		// Step 3: Propagate reduced result from previous block of TC elements
		val = OPERATOR_APPLY(val, reduceValue);
		
		// Step 4: Write out data to global memory
		dataSet[offsetIdx] = val;
		
		// Step 5: Choose reduced value for next iteration
		if (idx == (TC-1))
		{
			//localBuf[idx] = (Kind == exclusive) ? OPERATOR_APPLY(input, val) : val;
			localBuf[idx] = OPERATOR_APPLY(input, val);
		}
		barrier(CLK_LOCAL_MEM_FENCE);
		
		reduceValue = localBuf[TC-1];
		barrier(CLK_LOCAL_MEM_FENCE);
	}
}