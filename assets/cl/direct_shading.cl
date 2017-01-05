#ifndef __DIRECT_SHADING_CL
#define __DIRECT_SHADING_CL
#include "shading_helper.cl"

// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2007%20-%20path%20tracing.pdf
float3 slide17Shading(
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const float* normalTransform,
	float2 uv,
	image2d_array_t textures,
	clrngMrg31k3pStream* randomStream)
{
	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];
	if (material->type == Emissive)
		return material->emissive.emissiveColour;

	// Triangle normal
	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float3 normal = cross(
		vertices[1].vertex - vertices[0].vertex,
		vertices[2].vertex - vertices[0].vertex);
	normal = normalize(matrixMultiplyLocal(normalTransform, (float4)(normal, 0.0f)).xyz);

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
	float cos_o = dot(-L, lightNormal);
	float cos_i = dot(L, normal);
	if ((cos_o <= 0)  || (cos_i <= 0)) return BLACK;

	// Light is not behind surface point, trace shadow ray
	Ray ray;
	ray.origin = intersection + EPSILON * L;
	ray.direction = L;

	int bounceTriInd;
	bool hit = traceRay(scene, &ray, true, dist - 2 * EPSILON, &bounceTriInd, NULL, NULL, NULL);
	if (hit) {
		return BLACK;
	}

	float3 BRDF = material->diffuse.diffuseColour * INVPI;
	float solidAngle = (cos_o * lightArea) / dist2;
	solidAngle = min(solidAngle, 2 * PI);
	return scene->numEmissiveTriangles * BRDF * lightColour * solidAngle * cos_i;
}

// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2007%20-%20path%20tracing.pdf
float3 slide16Shading(
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const float* normalTransform,
	float2 uv,
	image2d_array_t textures,
	clrngMrg31k3pStream* randomStream)
{
	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];
	if (material->type == Emissive)
		return material->emissive.emissiveColour;

	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float3 e1 = vertices[1].vertex - vertices[0].vertex;
	float3 e2 = vertices[2].vertex - vertices[0].vertex;
	float3 normal = normalize(cross(e1, e2));
	normal = normalize(matrixMultiplyLocal(normalTransform, (float4)(normal, 0.0f)).xyz);

	if (dot(normal, -rayDirection) < 0.0f)
		return BLACK;

	float3 reflection = diffuseReflection(e1, e2, normalTransform, randomStream);
	Ray rayToHemisphere;
	rayToHemisphere.origin = intersection + reflection * EPSILON;
	rayToHemisphere.direction = reflection;

	int bounceTriInd;
	bool hit = traceRay(scene, &rayToHemisphere, false, INFINITY, &bounceTriInd, NULL, NULL, NULL);
	if (hit)
	{
		const __global Material* hitMat = &scene->meshMaterials[scene->triangles[bounceTriInd].mat_index];
		if (hitMat->type != Emissive)
			return BLACK;

		float3 BRDF = material->diffuse.diffuseColour * INVPI;
		float cos_i = dot(reflection, normal);
		return 2.0f * PI * BRDF * hitMat->emissive.emissiveColour * cos_i;
	} else {
		return BLACK;
	}
}

#endif// __DIRECT_SHADING_CL