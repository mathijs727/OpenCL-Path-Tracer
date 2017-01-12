#ifndef __SHADER_HELPER_CL
#define __SHADER_HELPER_CL
#include <clRNG/mrg31k3p.clh>
#include "math.cl"
#include "shapes.cl"
#include "scene.cl"

#define PI 3.14159265359f
#define INVPI 0.31830988618f;

#define BLACK (float3)(0, 0, 0);
#define WHITE (float3)(1, 1, 1);
#define GRAY(x) (float3)(x, x, x);

#define EPSILON 0.0001f

// Random int between start (inclusive) and stop (exclusive)
int randInt(clrngMrg31k3pStream* randomStream, int start, int stop)
{
	float u = min((float)clrngMrg31k3pRandomU01(randomStream), 1.0f - FLT_MAX);
	float range = stop - start;
	return start + (int)(u * range);
}

// http://www.rorydriscoll.com/2009/01/07/better-sampling/
float3 diffuseReflection(
	float3 edge1,
	float3 edge2,
	const float* normalTransform,
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

	// Transform hemisphere to normal of the surface (of the static model)
	// [tangent, bitangent, normal]
	float3 orientedSample = sample.x * tangent + sample.y * bitangent + sample.z * normal;
	
	// Apply the normal transform (top level BVH)
	orientedSample = normalize(matrixMultiplyLocal(normalTransform, (float4)(orientedSample, 0.0f)).xyz);
	return normalize(orientedSample);
}

// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 41
float3 cosineWeightedDiffuseReflection(
	float3 edge1,
	float3 edge2,
	const float* normalTransform,
	clrngMrg31k3pStream* randomStream)
{
	// A cosine-weither random distribution is obtained by generating points on the unit
	// disc, and projecting the disc on the unit hemisphere.
	float r0 = clrngMrg31k3pRandomU01(randomStream);
	float r1 = clrngMrg31k3pRandomU01(randomStream);
	float r = sqrt(r0);
	float theta = 2 * PI * r1;
	float x = r * cos(theta);
	float y = r * sin(theta);
	float3 sample = (float3)(x, y, sqrt(1 - r0));

	float3 normal = normalize(cross(edge1, edge2));
	float3 tangent = normalize(cross(normal, edge1));
	float3 bitangent = cross(normal, tangent);

	// Transform hemisphere to normal of the surface (of the static model)
	// [tangent, bitangent, normal]
	float3 orientedSample = sample.x * tangent + sample.y * bitangent + sample.z * normal;
	
	// Apply the normal transform (top level BVH)
	orientedSample = normalize(matrixMultiplyLocal(normalTransform, (float4)(orientedSample, 0.0f)).xyz);
	return normalize(orientedSample);
}

// http://stackoverflow.com/questions/19654251/random-point-inside-triangle-inside-java
float3 uniformSampleTriangle(const float3* vertices, clrngMrg31k3pStream* randomStream)
{
	float3 A = vertices[0];
	float3 B = vertices[1];
	float3 C = vertices[2];
	float u1 = clrngMrg31k3pRandomU01(randomStream);
	float u2 = clrngMrg31k3pRandomU01(randomStream);
	return (1 - sqrt(u1)) * A + (sqrt(u1) * (1 - u2)) * B + (sqrt(u1) * u2) * C;
}

// https://www.mathsisfun.com/geometry/herons-formula.html
float triangleArea(float3* vertices)
{
	float3 A = vertices[1] - vertices[0];
	float3 B = vertices[2] - vertices[1];
	float3 C = vertices[0] - vertices[2];
	float lenA = sqrt(dot(A, A));
	float lenB = sqrt(dot(B, B));
	float lenC = sqrt(dot(C, C));
	float s = (lenA + lenB + lenC) / 2.0f;
	return sqrt(s * (s - lenA) * (s - lenB) * (s - lenC));
}

void randomPointOnLight(
	const Scene* scene,
	clrngMrg31k3pStream* randomStream,
	float3* outPoint,
	float3* outLightNormal,
	float3* outLightColour,
	float* outLightArea)
{
	// Construct vector to random point on light
	int lightIndex = clrngMrg31k3pRandomInteger(randomStream, 0, scene->numEmissiveTriangles-1);
	EmissiveTriangle lightTriangle = scene->emissiveTriangles[lightIndex];
	*outLightNormal = normalize(cross(
		lightTriangle.vertices[1] - lightTriangle.vertices[0],
		lightTriangle.vertices[2] - lightTriangle.vertices[0]));
	*outLightColour = lightTriangle.material.emissive.emissiveColour;
	*outPoint = uniformSampleTriangle(lightTriangle.vertices, randomStream);
	*outLightArea = triangleArea(lightTriangle.vertices);
}

#endif// __SHADER_HELPER_CL