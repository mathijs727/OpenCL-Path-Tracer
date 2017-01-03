#ifndef __SHADING_CL
#define __SHADING_CL
#include "shading_helper.cl"

#define EPSILON 0.00001f

float3 shading(
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const float* normalTransform,
	float2 uv,
	image2d_array_t textures,
	clrngMrg31k3pStream* randomStream,
	float3 multiplier,
	Stack* stack)
{
	// Gather intersection data
	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float3 edge1 = vertices[1].vertex - vertices[0].vertex;
	float3 edge2 = vertices[2].vertex - vertices[0].vertex;
	float3 realNormal = normalize(cross(edge1, edge2));
	realNormal = normalize(matrixMultiplyLocal(normalTransform, (float4)(realNormal, 0.0f)).xyz);
	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];

	if (dot(realNormal, -rayDirection) < 0.0f)
		return BLACK;

	// Terminate if we hit a light source
	if (material->type == Emmisive)
		return multiplier * material->emmisive.emmisiveColour;

	// Continue in random direction
	float3 reflection = diffuseReflection(edge1, edge2, randomStream);
	reflection = normalize(matrixMultiplyLocal(normalTransform, (float4)(reflection, 0.0f)).xyz);

	// Update throughput
	float3 BRDF = material->diffuse.diffuseColour / PI;
	float3 Ei = dot(realNormal , reflection);// Irradiance
	float3 integral = PI * 2.0f * BRDF * Ei;
	StackPush(stack, intersection + reflection * EPSILON, reflection, multiplier * integral);
	return BLACK;
}

#endif// __SHADING_CL