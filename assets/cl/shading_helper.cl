#ifndef __SHADER_HELPER_CL
#define __SHADER_HELPER_CL

#define PI 3.14159265359f
#define INVPI 0.31830988618f;

#define BLACK (float3)(0, 0, 0);
#define WHITE (float3)(1, 1, 1);
#define GRAY(x) (float3)(x, x, x);

// http://www.rorydriscoll.com/2009/01/07/better-sampling/
float3 diffuseReflection(
	float3 edge1,
	float3 edge2,
	clrngMrg31k3pStream* randomStream)
{
	float u1 = clrngMrg31k3pRandomU01(randomStream);
	float u2 = clrngMrg31k3pRandomU01(randomStream);
	const float r = sqrt(1.0f - u1 * u1);
	const float phi = 2 * PI * u2;
	float3 sample = (float3)(cos(phi) * r, sin(phi) * r, u1);

	float3 normal = normalize(cross(edge1, edge2));
	float3 tangent = normalize(cross(normal, edge1));
	float3 bitangent = cross(normal, tangent);

	// Normal transform matrix
	// [tangent, bitangent, normal]
	float3 orientedSample = sample.x * tangent + sample.y * bitangent + sample.z * normal;
	return normalize(orientedSample);
}

// http://stackoverflow.com/questions/19654251/random-point-inside-triangle-inside-java
float3 uniformSampleTriangle(VertexData* vertices, clrngMrg31k3pStream* randomStream)
{
	float3 A = vertices[0].vertex;
	float3 B = vertices[1].vertex;
	float3 C = vertices[2].vertex;
	float u1 = clrngMrg31k3pRandomU01(randomStream);
	float u2 = clrngMrg31k3pRandomU01(randomStream);
	return (1 - sqrt(u1)) * A + (sqrt(u1) * (1 - u2)) * B + (sqrt(u1) * u2) * C;
}

// https://www.mathsisfun.com/geometry/herons-formula.html
float triangleArea(VertexData* vertices)
{
	float3 A = vertices[1].vertex - vertices[0].vertex;
	float3 B = vertices[2].vertex - vertices[1].vertex;
	float3 C = vertices[0].vertex - vertices[2].vertex;
	float lenA = sqrt(dot(A, A));
	float lenB = sqrt(dot(B, B));
	float lenC = sqrt(dot(C, C));
	float s = (lenA + lenB + lenC) / 2.0f;
	return sqrt(s * (s - lenA) * (s - lenB) * (s - lenC));
}

#endif// __SHADER_HELPER_CL