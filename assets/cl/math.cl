#ifndef __math_cl
#define __math_cl

float4 matrixMultiply(const float* matrix, float4 vector)
{
	/*float4 col1 = vector.x * vload4(0, matrix);
	float4 col2 = vector.y * vload4(4, matrix);
	float4 col3 = vector.z * vload4(8, matrix);
	float4 col4 = vector.w * vload4(12, matrix);*/
	float4 col1 = vector.x * (float4)(matrix[0+0*4], matrix[1+0*4], matrix[2+0*4], matrix[3+0*4]);
	float4 col2 = vector.y * (float4)(matrix[0+1*4], matrix[1+1*4], matrix[2+1*4], matrix[3+1*4]);
	float4 col3 = vector.z * (float4)(matrix[0+2*4], matrix[1+2*4], matrix[2+2*4], matrix[3+2*4]);
	float4 col4 = vector.w * (float4)(matrix[0+3*4], matrix[1+3*4], matrix[2+3*4], matrix[3+3*4]);
	return col1 + col2 + col3 + col4;
}

#endif// __math_cl