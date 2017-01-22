#ifndef __math_cl
#define __math_cl

float4 matrixMultiply(const __global float* matrix, float4 vector)
{
	float4 col1 = vector.x * (float4)(matrix[0+0*4], matrix[1+0*4], matrix[2+0*4], matrix[3+0*4]);
	float4 col2 = vector.y * (float4)(matrix[0+1*4], matrix[1+1*4], matrix[2+1*4], matrix[3+1*4]);
	float4 col3 = vector.z * (float4)(matrix[0+2*4], matrix[1+2*4], matrix[2+2*4], matrix[3+2*4]);
	float4 col4 = vector.w * (float4)(matrix[0+3*4], matrix[1+3*4], matrix[2+3*4], matrix[3+3*4]);
	return col1 + col2 + col3 + col4;
}

float4 matrixMultiplyLocal(const float* matrix, float4 vector)
{
	float4 col1 = vector.x * (float4)(matrix[0+0*4], matrix[1+0*4], matrix[2+0*4], matrix[3+0*4]);
	float4 col2 = vector.y * (float4)(matrix[0+1*4], matrix[1+1*4], matrix[2+1*4], matrix[3+1*4]);
	float4 col3 = vector.z * (float4)(matrix[0+2*4], matrix[1+2*4], matrix[2+2*4], matrix[3+2*4]);
	float4 col4 = vector.w * (float4)(matrix[0+3*4], matrix[1+3*4], matrix[2+3*4], matrix[3+3*4]);
	return col1 + col2 + col3 + col4;
}

float3 matrixMultiplyTranspose(const __global float* matrix, float3 vector)
{
	float3 col1 = vector.x * (float3)(matrix[0+0*4], matrix[0+1*4], matrix[0+2*4]);//, matrix[0+3*4]);
	float3 col2 = vector.y * (float3)(matrix[1+0*4], matrix[1+1*4], matrix[1+2*4]);//, matrix[1+3*4]);
	float3 col3 = vector.z * (float3)(matrix[2+0*4], matrix[2+1*4], matrix[2+2*4]);//, matrix[2+3*4]);
	//float4 col4 = vector.w * (float4)(matrix[3+0*4], matrix[3+1*4], matrix[3+2*4]);//, matrix[3+3*4]);
	return col1 + col2 + col3;// + col4;
}

void matrixTranspose(const __global float* matrix, float* matrixOut)
{
	for (int col = 0; col < 4; col++)
	{
		for (int row = 0; row < 4; row++)
		{
			matrixOut[row + col * 4] = matrix[col + row * 4];
		}
	}
}

void matrixTransposeLocal(const float* matrix, float* matrixOut)
{
	for (int col = 0; col < 4; col++)
	{
		for (int row = 0; row < 4; row++)
		{
			matrixOut[row + col * 4] = matrix[col + row * 4];
		}
	}
}

float lerp(float x, float y, float a)
{
	return mix(x, y, a);
}

float saturate(float a)
{
	return min(1.0f, max(0.0f, a));
}

#endif// __math_cl