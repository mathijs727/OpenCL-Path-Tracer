#ifndef __SHADING_CL
#define __SHADING_CL
#include "shading_helper.cl"

// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 26
float3 neeShading(
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const float* normalTransform,
	float2 uv,
	image2d_array_t textures,
	clrngMrg31k3pStream* randomStream,
	StackItem* stackItem,
	Stack* stack)
{
	// Gather intersection data
	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float3 edge1 = vertices[1].vertex - vertices[0].vertex;
	float3 edge2 = vertices[2].vertex - vertices[0].vertex;
	float3 realNormal = cross(edge1, edge2);
	realNormal = normalize(matrixMultiplyLocal(normalTransform, (float4)(realNormal, 0.0f)).xyz);
	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];

	if (dot(realNormal, -rayDirection) < 0.0f)
		return BLACK;

	float3 BRDF = material->diffuse.diffuseColour * INVPI;

	// Terminate if we hit a light source
	if (material->type == Emmisive)
	{
		if (stackItem->lastSpecular) {
			return stackItem->multiplier * material->emmisive.emmisiveColour;
		} else {
			return BLACK;
		}
	}

	// Sample a random light source
	float3 lightPos, lightNormal, lightColour; float lightArea;
	randomPointOnLight(
		scene,
		randomStream,
		&lightPos,
		&lightNormal,
		&lightColour,
		&lightArea);
	float3 L = lightPos - intersection;
	float dist2 = dot(L, L);
	float dist = sqrt(dist2);
	L /= dist;

	float3 Ld = BLACK;
	Ray lightRay = createRay(intersection + L * EPSILON, L);
	if (dot(realNormal, L) > 0.0f && dot(lightNormal, -L) > 0.0f)
	{
		if (!traceRay(scene, &lightRay, true, dist - 2 * EPSILON, NULL, NULL, NULL, NULL))
		{
			float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
			solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
			Ld = scene->numEmmisiveTriangles * lightColour * solidAngle * BRDF * dot(realNormal, L);
		}
	}

	// Continue random walk
	float3 reflection = diffuseReflection(edge1, edge2, normalTransform, randomStream);

	float3 Ei = dot(realNormal, reflection);
	float3 integral = PI * 2.0f * BRDF * Ei;
	StackPushNEE(
		stack,
		intersection + reflection * EPSILON,
		reflection,
		stackItem->multiplier * integral,
		false);
	return stackItem->multiplier * Ld;
}



float3 naiveShading(
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const float* normalTransform,
	float2 uv,
	image2d_array_t textures,
	clrngMrg31k3pStream* randomStream,
	StackItem* stackItem,
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
		return stackItem->multiplier * material->emmisive.emmisiveColour;

	// Continue in random direction
	float3 reflection = diffuseReflection(edge1, edge2, normalTransform, randomStream);

	// Update throughput
	float3 BRDF = material->diffuse.diffuseColour / PI;
	float3 Ei = dot(realNormal , reflection);// Irradiance
	float3 integral = PI * 2.0f * BRDF * Ei;
	StackPush(stack, intersection + reflection * EPSILON, reflection, stackItem->multiplier * integral);
	return BLACK;
}

#endif// __SHADING_CL