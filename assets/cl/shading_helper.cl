#ifndef __SHADER_HELPER_CL
#define __SHADER_HELPER_CL
#include <clRNG/lfsr113.clh>
#include "math.cl"
#include "shapes.cl"
#include "scene.cl"

#define PI 3.14159265359f
#define INVPI 0.31830988618f;

#define BLACK (float3)(0, 0, 0);
#define WHITE (float3)(1, 1, 1);
#define GRAY(x) (float3)(x, x, x);

#define EPSILON 0.0001f

__constant sampler_t sampler =
	CLK_NORMALIZED_COORDS_TRUE |
	CLK_ADDRESS_REPEAT |
	CLK_FILTER_LINEAR;



// Random int between start (inclusive) and stop (exclusive)
int randInt(clrngLfsr113Stream* randomStream, int start, int stop)
{
	float u = min((float)clrngLfsr113RandomU01(randomStream), 1.0f - FLT_MAX);
	float range = stop - start;
	return start + (int)(u * range);
}

// http://www.rorydriscoll.com/2009/01/07/better-sampling/
float3 diffuseReflection(
	float3 edge1,
	float3 edge2,
	const __global float* invTransform,
	clrngLfsr113Stream* randomStream)
{
	float u1 = clrngLfsr113RandomU01(randomStream);
	float u2 = clrngLfsr113RandomU01(randomStream);
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
	orientedSample = normalize(matrixMultiplyTranspose(invTransform, orientedSample));
	return normalize(orientedSample);
}

// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 41
float3 cosineWeightedDiffuseReflection(
	float3 edge1,
	float3 edge2,
	const __global float* invTransform,
	clrngLfsr113Stream* randomStream)
{
	// A cosine-weither random distribution is obtained by generating points on the unit
	// disc, and projecting the disc on the unit hemisphere.
	float r0 = clrngLfsr113RandomU01(randomStream);
	float r1 = clrngLfsr113RandomU01(randomStream);
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
	orientedSample = normalize(matrixMultiplyTranspose(invTransform, orientedSample));
	return normalize(orientedSample);
}

// http://stackoverflow.com/questions/19654251/random-point-inside-triangle-inside-java
float3 uniformSampleTriangle(const float3* vertices, clrngLfsr113Stream* randomStream)
{
	float3 A = vertices[0];
	float3 B = vertices[1];
	float3 C = vertices[2];
	float u1 = clrngLfsr113RandomU01(randomStream);
	float u2 = clrngLfsr113RandomU01(randomStream);
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

void weightedRandomPointOnLight(
	const Scene* scene,
	float3 intersection,
	clrngLfsr113Stream* randomStream,
	float3* outPoint,
	float3* outLightNormal,
	float3* outLightColour,
	float* outLightArea)
{
	int numLights = scene->numEmissiveTriangles;
	float weightTotal = 0;
	float _lightWeights[255];
	for (int i = 0; i < numLights; ++i) {
		EmissiveTriangle lightTriangle = scene->emissiveTriangles[i];
		float3 lightPos = (lightTriangle.vertices[2] + lightTriangle.vertices[1] + lightTriangle.vertices[0]) / 3;
		float3 L = lightPos - intersection;
		float dist2 = dot(L, L);
		float dist = sqrt(dist2);
		L /= dist;

		float3 lightNormal = normalize(cross(
		lightTriangle.vertices[1] - lightTriangle.vertices[0],
		lightTriangle.vertices[2] - lightTriangle.vertices[0]));
		float lightArea = triangleArea(lightTriangle.vertices);
		float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;		
		solidAngle = min(2 * PI, solidAngle);
		_lightWeights[i] = solidAngle;
		weightTotal += _lightWeights[i];
	}
	float randomValue = clrngLfsr113RandomU01(randomStream) * weightTotal;
	int lightIndex;
	for (lightIndex = 0; lightIndex < numLights; ++lightIndex) {
		randomValue -= _lightWeights[lightIndex];
		if (randomValue <= 0) break;
	}
	// Construct vector to random point on light
	EmissiveTriangle lightTriangle = scene->emissiveTriangles[lightIndex];
	*outLightNormal = normalize(cross(
		lightTriangle.vertices[1] - lightTriangle.vertices[0],
		lightTriangle.vertices[2] - lightTriangle.vertices[0]));
	*outLightColour = lightTriangle.material.emissive.emissiveColour * weightTotal / numLights;
	*outPoint = uniformSampleTriangle(lightTriangle.vertices, randomStream);
	*outLightArea = triangleArea(lightTriangle.vertices);
}

void randomPointOnLight(
	const Scene* scene,
	clrngLfsr113Stream* randomStream,
	float3* outPoint,
	float3* outLightNormal,
	float3* outLightColour,
	float* outLightArea)
{
	// Construct vector to random point on light
	int lightIndex = clrngLfsr113RandomInteger(randomStream, 0, scene->numEmissiveTriangles-1);
	EmissiveTriangle lightTriangle = scene->emissiveTriangles[lightIndex];
	*outLightNormal = normalize(cross(
		lightTriangle.vertices[1] - lightTriangle.vertices[0],
		lightTriangle.vertices[2] - lightTriangle.vertices[0]));
	*outLightColour = lightTriangle.material.emissive.emissiveColour;
	*outPoint = uniformSampleTriangle(lightTriangle.vertices, randomStream);
	*outLightArea = triangleArea(lightTriangle.vertices);
}

float3 diffuseColour(
	const __global Material* material,
	VertexData* vertices,
	float2 uv,
	image2d_array_t textures)
{
	if (material->diffuse.tex_id == -1)
	{
		return material->diffuse.diffuseColour;
	} else {
		float2 t0 = vertices[0].texCoord;
		float2 t1 = vertices[1].texCoord;
		float2 t2 = vertices[2].texCoord;
		float2 tex_coords = t0 + (t1-t0) * uv.x + (t2-t0) * uv.y;

		float4 texCoords3d = (float4)(tex_coords.x, 1.0f - tex_coords.y, material->diffuse.tex_id, 0.0f);
		float4 colourWithAlpha = read_imagef(
			textures,
			sampler,
			texCoords3d);
		return colourWithAlpha.xyz;
	}
}

float3 interpolateNormal(const VertexData* vertices, float2 uv)
{
	float3 n0 = vertices[0].normal;
	float3 n1 = vertices[1].normal;
	float3 n2 = vertices[2].normal;
	return normalize(n0 + (n1-n0) * uv.x + (n2-n0) * uv.y);
}

#endif// __SHADER_HELPER_CL