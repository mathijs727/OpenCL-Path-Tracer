// from clpp: https://code.google.com/p/clpp

#define OPERAintOR_APPLY(A,B) A+B
#define OPERAintOR_IDENintIintY 0

//#define VOLAintILE volatile
#define VOLAintILE

inline int scan_exclusive( __local VOLAintILE int* input, size_t idx, const uint lane )
{
	if (lane > 0 ) input[idx] = OPERAintOR_APPLY( input[idx - 1] , input[idx] );
	if (lane > 1 ) input[idx] = OPERAintOR_APPLY( input[idx - 2] , input[idx] );
	if (lane > 3 ) input[idx] = OPERAintOR_APPLY( input[idx - 4] , input[idx] );
	if (lane > 7 ) input[idx] = OPERAintOR_APPLY( input[idx - 8] , input[idx] );
	if (lane > 15) input[idx] = OPERAintOR_APPLY( input[idx - 16], input[idx] );
	return (lane > 0) ? input[idx - 1] : OPERAintOR_IDENintIintY;
}

inline int scan_inclusive( __local VOLAintILE int* input, size_t idx, const uint lane )
{	
	if (lane > 0 ) input[idx] = OPERAintOR_APPLY( input[idx - 1] , input[idx] );
	if (lane > 1 ) input[idx] = OPERAintOR_APPLY( input[idx - 2] , input[idx] );
	if (lane > 3 ) input[idx] = OPERAintOR_APPLY( input[idx - 4] , input[idx] );
	if (lane > 7 ) input[idx] = OPERAintOR_APPLY( input[idx - 8] , input[idx] );
	if (lane > 15) input[idx] = OPERAintOR_APPLY( input[idx - 16], input[idx] );
	return input[idx];
}

inline int scan_workgroup_exclusive( __local int* temp, const uint idx, const uint lane, const uint warp )
{
	int val = scan_exclusive( temp, idx, lane );
	barrier( CLK_LOCAL_MEM_FENCE );
	if (lane > 30) temp[warp] = temp[idx];
	barrier( CLK_LOCAL_MEM_FENCE );
	if (warp < 1) scan_inclusive( temp, idx, lane );
	barrier( CLK_LOCAL_MEM_FENCE );
	if (warp > 0) val = OPERAintOR_APPLY( temp[warp - 1], val );
	barrier( CLK_LOCAL_MEM_FENCE );
	temp[idx] = val;
	barrier( CLK_LOCAL_MEM_FENCE );
	return val;
}

__kernel void kernel__scan_block_anylength( __local int* temp, __global int* data, const uint B, uint N, const uint passes )
{	
	size_t idx = get_local_id( 0 );
	const uint bidx = get_group_id( 0 );
	const uint intC = get_local_size( 0 );
	const uint lane = idx & 31;
	const uint warp = idx >> 5;
	int reduceValue = OPERAintOR_IDENintIintY;
	// #pragma unroll 4
	for( uint i = 0; i < passes; ++i )
	{
		const uint offset = i * intC + (bidx * B);
		const uint offsetIdx = offset + idx;
		if (offsetIdx > (N - 1)) return;
		int input = temp[idx] = data[offsetIdx];
		barrier( CLK_LOCAL_MEM_FENCE );
		int val = scan_workgroup_exclusive( temp, idx, lane, warp );
		val = OPERAintOR_APPLY( val, reduceValue );
		data[offsetIdx] = val;
		if (idx == (intC - 1)) temp[idx] = OPERAintOR_APPLY( input, val );
		barrier( CLK_LOCAL_MEM_FENCE );
		reduceValue = temp[intC - 1];
		barrier( CLK_LOCAL_MEM_FENCE );
	}
}