#ifndef _RANDOM_CL
#define _RANDOM_CL

// http://stackoverflow.com/questions/9912143/how-to-get-a-random-number-in-opencl
// http://www0.cs.ucl.ac.uk/staff/ucacbbl/ftp/papers/langdon_2009_CIGPU.pdf
int randInt(int* seed) // 1 <= *seed < m
{
	int const a = 16807; //ie 7**5
	int const m = 2147483647; //ie 2**31-1

	int id = get_local_id(0) * get_local_size(1) + get_local_id(1);
	__local int seedBuffer[MAX_GROUP_SIZE];
	seedBuffer[id] = 0;
	*seed = (long(*seed * a))%m;
	return(*seed);
}

// http://www0.cs.ucl.ac.uk/staff/ucacbbl/ftp/papers/langdon_2009_CIGPU.pdf
float randFloat()
{
	float const a = 16807; //ie 7**5
	float const m = 2147483647; //ie 2**31-1
	float const reciprocal_m = 1.0 / m;

	float temp = seed * a;
	float result = temp - m * floor(temp * reciprocal_m);
	*seed = (int)result;
}

#endif// _RANDOM_CL