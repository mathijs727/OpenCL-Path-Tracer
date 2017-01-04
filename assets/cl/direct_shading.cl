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
	if (material->type == Emmisive)
		return material->emmisive.emmisiveColour;

	// Triangle normal
	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float3 normal = normalize(cross(
		vertices[1].vertex - vertices[0].vertex,
		vertices[2].vertex - vertices[0].vertex));

	// Construct vector to random point on light
	int lightIndex = clrngMrg31k3pRandomInteger(randomStream, 0, scene->numEmmisiveTriangles);
	TriangleData lightTriangle = scene->triangles[scene->emmisiveTriangles[lightIndex]];
	VertexData lightVertices[3];
	getVertices(lightVertices, lightTriangle.indices, scene);
	float3 lightNormal = normalize(cross(
		lightVertices[1].vertex - lightVertices[0].vertex,
		lightVertices[2].vertex - lightVertices[0].vertex));
	float3 lightColour = scene->meshMaterials[lightTriangle.mat_index].emmisive.emmisiveColour;

	float3 L = uniformSampleTriangle(lightVertices, randomStream) - intersection;
	float dist = sqrt(dot(L, L));
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
	if (hit)
		return BLACK;


	float3 BRDF = material->diffuse.diffuseColour * INVPI;
	float solidAngle = (cos_o * triangleArea(lightVertices)) / (dist * dist);
	solidAngle = min(2 * PI, solidAngle);
	return scene->numEmmisiveTriangles * BRDF * lightColour * solidAngle * cos_i;
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
	if (material->type == Emmisive)
		return material->emmisive.emmisiveColour;

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
		if (hitMat->type != Emmisive)
			return BLACK;

		float3 BRDF = material->diffuse.diffuseColour * INVPI;
		float cos_i = dot(reflection, normal);
		return 2.0f * PI * BRDF * hitMat->emmisive.emmisiveColour * cos_i;
	} else {
		return BLACK;
	}
}

#endif// __DIRECT_SHADING_CL