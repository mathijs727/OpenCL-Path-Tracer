#ifndef __SHADING_CL
#define __SHADING_CL

#define EPSILON 0.0001f
#define PI 3.14159265359f
#define INVPI 0.31830988618f;

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

float3 slide17Shading(
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const __global float* invTransform,
	float2 uv,
	image2d_array_t textures,
	clrngMrg31k3pStream* randomStream)
{

}

float3 slide16Shading(
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const __global float* invTransform,
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
	float2 t0 = vertices[0].texCoord;
	float2 t1 = vertices[1].texCoord;
	float2 t2 = vertices[2].texCoord;
	float2 tex_coords = t0 + (t1-t0) * uv.x + (t2-t0) * uv.y;
	float3 n0 = vertices[0].normal;
	float3 n1 = vertices[1].normal;
	float3 n2 = vertices[2].normal;
	float3 normal = normalize( n0 + (n1-n0) * uv.x + (n2-n0) * uv.y );
	float3 e1 = vertices[1].vertex - vertices[0].vertex;
	float3 e2 = vertices[2].vertex - vertices[0].vertex;

	float normalTransform[16];
	matrixTranspose(invTransform, normalTransform);
	normal = normalize(matrixMultiplyLocal(normalTransform, (float4)(normal, 0.0f)).xyz);

	if (dot(normal, -rayDirection) < 0.0f)
		return (float3)(0, 0, 0);

	float3 reflection = diffuseReflection(e1, e2, randomStream);
	Ray rayToHemisphere;
	rayToHemisphere.origin = intersection + reflection * EPSILON;
	rayToHemisphere.direction = reflection;

	int tmpTriInd;
	float tmpT;
	float2 tmpUv;
	const __global float* tmpMatrix;
	bool hit = traceRay(scene, &rayToHemisphere, false, INFINITY, &tmpTriInd, &tmpT, &tmpUv, &tmpMatrix);
	if (hit)
	{
		const __global Material* hitMat = &scene->meshMaterials[scene->triangles[tmpTriInd].mat_index];
		if (hitMat->type != Emmisive)
			return (float3)(0,0,0);

		float3 BRDF = material->diffuse.diffuseColour * INVPI;
		float cos_i = dot(reflection, normal);
		return 2.0f * PI * BRDF * hitMat->emmisive.emmisiveColour * cos_i;
	} else {
		return (float3)(0,0,0);
	}
}

#endif// __SHADING_CL