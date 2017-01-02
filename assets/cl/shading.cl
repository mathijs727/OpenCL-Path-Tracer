#ifndef __SHADING_CL
#define __SHADING_CL

#define EPSILON 0.0001f
#define PI 3.14159265359f
#define INVPI 0.31830988618f;
#define BLACK (float3)(0, 0, 0);
#define WHITE (float3)(1, 1, 1);

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
	float3 A = vertices[0].vertex;
	float3 B = vertices[1].vertex;
	float3 C = vertices[2].vertex;
	float lenA = sqrt(dot(A, A));
	float lenB = sqrt(dot(B, B));
	float lenC = sqrt(dot(C, C));
	float s = (lenA + lenB + lenC) / 2.0f;
	return sqrt(s * (s - lenA) * (s - lenB) * (s - lenC));
}

// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2007%20-%20path%20tracing.pdf
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
	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];
	if (material->type == Emmisive)
		return material->emmisive.emmisiveColour;

	// Construct vector to random point on light
	int lightIndex = clrngMrg31k3pRandomInteger(randomStream, 0, scene->numEmmisiveTriangles);
	TriangleData lightTriangle = scene->triangles[scene->emmisiveTriangles[lightIndex]];
	VertexData lightVertices[3];
	getVertices(lightVertices, lightTriangle.indices, scene);
	float3 edge1 = lightVertices[1].vertex - lightVertices[0].vertex;
	float3 edge2 = lightVertices[2].vertex - lightVertices[0].vertex;
	float3 lightNormal = normalize(cross(edge1, edge2));
	float3 lightColour = scene->meshMaterials[lightTriangle.mat_index].emmisive.emmisiveColour;

	float3 L = normalize(uniformSampleTriangle(lightVertices, randomStream) - intersection);
	//float3 L = lightVertices[0].vertex - intersection;
	float dist = sqrt(dot(L, L));
	L /= dist;
	float cos_o = dot(-L, lightNormal);
	float cos_i = dot(L, -rayDirection);
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
	return BRDF * scene->numEmmisiveTriangles * lightColour * solidAngle * cos_i;
}

// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2007%20-%20path%20tracing.pdf
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
		return BLACK;

	float3 reflection = diffuseReflection(e1, e2, randomStream);
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

#endif// __SHADING_CL