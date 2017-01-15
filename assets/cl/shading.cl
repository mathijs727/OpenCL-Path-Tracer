#ifndef __SHADING_CL
#define __SHADING_CL
#include "shading_helper.cl"
#include <clRNG/mrg31k3p.clh>
#include "light.cl"

__constant sampler_t sampler =
	CLK_NORMALIZED_COORDS_TRUE |
	CLK_ADDRESS_REPEAT |
	CLK_FILTER_LINEAR;

enum {
	SHADINGFLAGS_HASFINISHED = 1,
	SHADINGFLAGS_LASTSPECULAR = 2
};

typedef struct {
	Ray ray;// 2 * float3 = 32 bytes
	float3 multiplier;// 16 bytes
	size_t outputPixel;// 4/8 bytes?
	int flags;// 4 bytes
	union
	{
		float rayLength;// 4 bytes
		int numBounces;
	};
	// Aligned to 16 bytes so struct has size of 64 bytes
} ShadingData;


// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 49/51
float3 neeMisShading(// Next Event Estimation + Multiple Importance Sampling
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const float* normalTransform,
	float2 uv,
	image2d_array_t textures,
	clrngMrg31k3pStream* randomStream,
	ShadingData* data)
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
	{
		data->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	float3 BRDF = material->diffuse.diffuseColour * INVPI;

	// Terminate if we hit a light source
	if (material->type == Emissive)
	{
		data->flags |= SHADINGFLAGS_HASFINISHED;
		if (data->flags & SHADINGFLAGS_LASTSPECULAR) {
			return data->multiplier * material->emissive.emissiveColour;
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
			float lightPDF = 1 / solidAngle;
			float brdfPDF = dot(realNormal, L) / PI;//(1 / (2 * PI));
			float misPDF = lightPDF + brdfPDF;
			Ld = scene->numEmissiveTriangles * (dot(realNormal, L) / misPDF) * BRDF * lightColour;
		}
	}

	// Continue random walk
	data->flags = 0;
	float3 reflection = cosineWeightedDiffuseReflection(edge1, edge2, normalTransform, randomStream);
	//float lightPdf = dot(realNormal, reflection) / PI;
	//float3 integral = (dot(realNormal, reflection) / lightPdf) * BRDF;
	float3 integral = PI * BRDF;
	float3 oldMultiplier = data->multiplier;
	data->ray.origin = intersection + reflection * EPSILON;
	data->ray.direction = reflection;
	data->multiplier = oldMultiplier * integral;
	return oldMultiplier * Ld;
}




// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 42
float3 neeIsShading(// Next Event Estimation + Importance Sampling
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const float* normalTransform,
	float2 uv,
	image2d_array_t textures,
	clrngMrg31k3pStream* randomStream,
	ShadingData* data)
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
	{
		data->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	float3 BRDF = material->diffuse.diffuseColour * INVPI;

	// Terminate if we hit a light source
	if (material->type == Emissive)
	{
		data->flags |= SHADINGFLAGS_HASFINISHED;
		if (data->flags & SHADINGFLAGS_LASTSPECULAR) {
			return data->multiplier * material->emissive.emissiveColour;
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
			Ld = scene->numEmissiveTriangles * lightColour * solidAngle * BRDF * dot(realNormal, L);
		}
	}

	// Continue random walk
	data->flags = 0;
	float3 reflection = cosineWeightedDiffuseReflection(edge1, edge2, normalTransform, randomStream);
	//float PDF = dot(realNormal, reflection) / PI;
	//float3 Ei = dot(realNormal, reflection) / PDF;
	float3 Ei = PI;
	float3 integral = BRDF * Ei;
	float3 oldMultiplier = data->multiplier;
	data->ray.origin = intersection + reflection * EPSILON;
	data->ray.direction = reflection;
	data->multiplier = oldMultiplier * integral;
	return oldMultiplier * Ld;
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
	ShadingData* inData,
	ShadingData* outData,
	ShadingData* outShadowData)
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
	
	// Terminate if we hit a light source
	if (material->type == Emissive)
	{
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		if (inData->flags & SHADINGFLAGS_LASTSPECULAR) {
			return inData->multiplier * material->emissive.emissiveColour;
		} else {
			return BLACK;
		}
	}

	if (dot(realNormal, -rayDirection) < 0.0f)
	{
		// Stop if we hit the back side
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	float3 BRDF = diffuseColour(material, vertices, uv, textures) * INVPI;

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

	//float3 Ld = BLACK;
	//Ray lightRay = createRay(intersection + L * EPSILON, L);
	if (dot(realNormal, L) > 0.0f && dot(lightNormal, -L) > 0.0f)
	{
		/*if (!traceRay(scene, &lightRay, true, dist - 2 * EPSILON, NULL, NULL, NULL, NULL))
		{
			float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
			solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
			Ld = scene->numEmissiveTriangles * lightColour * solidAngle * BRDF * dot(realNormal, L);
		}*/
		float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
		solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
		float3 Ld = scene->numEmissiveTriangles * lightColour * solidAngle * BRDF * dot(realNormal, L);
		outShadowData->multiplier = Ld * inData->multiplier;
		outShadowData->ray = createRay(intersection + L * EPSILON, L);
		outShadowData->rayLength = dist - 2 * EPSILON;
	} else {
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
	}

	// Continue random walk
	float3 reflection = diffuseReflection(edge1, edge2, normalTransform, randomStream);
	outData->flags = 0;
	float3 Ei = dot(realNormal, reflection);
	float3 integral = PI * 2.0f * BRDF * Ei;
	outData->ray.origin = intersection + reflection * EPSILON;
	outData->ray.direction = reflection;
	outData->multiplier = inData->multiplier * integral;
	return BLACK;
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
	ShadingData* data)
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
	{
		data->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	// Terminate if we hit a light source
	if (material->type == Emissive)
	{
		data->flags = SHADINGFLAGS_HASFINISHED;
		return data->multiplier * material->emissive.emissiveColour;
	}

	// Continue in random direction
	float3 reflection = diffuseReflection(edge1, edge2, normalTransform, randomStream);

	// Update throughput
	data->flags = 0;
	float3 BRDF = material->diffuse.diffuseColour / PI;
	float3 Ei = dot(realNormal , reflection);// Irradiance
	float3 integral = PI * 2.0f * BRDF * Ei;
	float3 oldMultiplier = data->multiplier;
	data->ray.origin = intersection + reflection * EPSILON;
	data->ray.direction = reflection;
	data->multiplier = oldMultiplier * integral;
	return BLACK;
}

#endif// __SHADING_CL