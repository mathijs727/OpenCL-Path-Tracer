#ifndef __SHADING_CL
#define __SHADING_CL
#include "shading_helper.cl"
#include <clRNG/lfsr113.clh>
#include "light.cl"
#include "pbr_brdf.cl"

enum {
	SHADINGFLAGS_HASFINISHED = 1,
	SHADINGFLAGS_LASTSPECULAR = 2
};

typedef struct {
	Ray ray;// 2 * float3 = 32 bytes
	float3 multiplier;// 16 bytes
	size_t outputPixel;// 4/8 bytes?
	int flags;// 4 bytes
	union// 4 bytes
	{
		float rayLength;// Only shadows use this
		int numBounces;// And shadows dont bounce
	};
	// Aligned to 16 bytes so struct has size of 64 bytes
} RayData;


// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 49/51
float3 neeMisShading(// Next Event Estimation + Multiple Importance Sampling
	const __local Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const __global float* invTransform,
	float2 uv,
	image2d_array_t textures,
	clrngLfsr113Stream* randomStream,
	const __global RayData* inData,
	RayData* outData,
	RayData* outShadowData)
{
	// Gather intersection data
	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float3 edge1 = vertices[1].vertex - vertices[0].vertex;
	float3 edge2 = vertices[2].vertex - vertices[0].vertex;
	float3 realNormal = cross(edge1, edge2);
	realNormal = normalize(matrixMultiplyTranspose(invTransform, realNormal));
	float3 shadingNormal = interpolateNormal(vertices, uv);
	//realNormal = shadingNormal;

	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];

	// Terminate if we hit a light source
	if (material->type == Emissive)
	{
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		if (inData->flags & SHADINGFLAGS_LASTSPECULAR) {
			return inData->multiplier * material->emissive.emissiveColour;
		}
		else {
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

	// Sample a random light source
	float3 lightPos, lightNormal, lightColour; float lightArea;
	weightedRandomPointOnLight(
		scene,
		intersection,
		randomStream,
		&lightPos,
		&lightNormal,
		&lightColour,
		&lightArea);
	float3 L = lightPos - intersection;
	float dist2 = dot(L, L);
	float dist = sqrt(dist2);
	L /= dist;

	float3 BRDF = 0.0f;
	if (dot(realNormal, L) > 0.0f && dot(lightNormal, -L) > 0.0f)
	{
		if (material->type == PBR) {
			BRDF = pbrBrdf(normalize(-rayDirection), L, shadingNormal, material, randomStream);
		}
		else if (material->type == Diffuse)
		{
			BRDF = diffuseColour(material, vertices, uv, textures) / PI;
		}

		float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
		solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
		float3 Ld = scene->numEmissiveTriangles * lightColour * BRDF * dot(realNormal, L);
		outShadowData->flags = 0;
		outShadowData->multiplier = Ld * inData->multiplier;
		outShadowData->ray = createRay(intersection + L * EPSILON, L);
		outShadowData->rayLength = dist - 2 * EPSILON;
	}
	else {
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
	}


	float PDF;
	float3 reflection;
	if (material->type == PBR) {
		//reflection = cosineWeightedDiffuseReflection(edge1, edge2, invTransform, randomStream);
		//PDF = dot(realNormal, reflection) / PI;
		reflection = ggxWeightedImportanceDirection(edge1, edge2, rayDirection, invTransform, 1 - material->pbr.smoothness, randomStream, &PDF);
		BRDF = pbrBrdf(normalize(-rayDirection), reflection, shadingNormal, material, randomStream);
	}
	else if (material->type == Diffuse) {
		reflection = cosineWeightedDiffuseReflection(edge1, edge2, invTransform, randomStream);
		PDF = dot(realNormal, reflection) / PI;
		BRDF = diffuseColour(material, vertices, uv, textures) / PI;
	}

	if (dot(realNormal, reflection) < 0.0f)
	{
		// Stop if we have a ray going inside
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	// Continue random walk
	outData->flags = 0;
	float3 integral = BRDF * dot(realNormal, reflection) / PDF;
	outData->ray.origin = intersection + reflection * EPSILON;
	outData->ray.direction = reflection;
	outData->multiplier = inData->multiplier * integral;
	return BLACK;
}




// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 42
float3 neeIsShading(// Next Event Estimation + Importance Sampling
	const __local Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const __global float* invTransform,
	float2 uv,
	image2d_array_t textures,
	clrngLfsr113Stream* randomStream,
	const __global RayData* inData,
	RayData* outData,
	RayData* outShadowData)
{
	// Gather intersection data
	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float3 edge1 = vertices[1].vertex - vertices[0].vertex;
	float3 edge2 = vertices[2].vertex - vertices[0].vertex;
	float3 realNormal = cross(edge1, edge2);
	realNormal = normalize(matrixMultiplyTranspose(invTransform, realNormal));
	float3 shadingNormal = interpolateNormal(vertices, uv);
	//realNormal = shadingNormal;

	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];

	// Terminate if we hit a light source
	if (material->type == Emissive)
	{
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		if (inData->flags & SHADINGFLAGS_LASTSPECULAR) {
			return inData->multiplier * material->emissive.emissiveColour;
		}
		else {
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

	// Sample a random light source
	float3 lightPos, lightNormal, lightColour; float lightArea;
	weightedRandomPointOnLight(
		scene,
		intersection,
		randomStream,
		&lightPos,
		&lightNormal,
		&lightColour,
		&lightArea);
	float3 L = lightPos - intersection;
	float dist2 = dot(L, L);
	float dist = sqrt(dist2);
	L /= dist;

	float3 BRDF = 0.0f;
	if (dot(realNormal, L) > 0.0f && dot(lightNormal, -L) > 0.0f)
	{
		if (material->type == PBR) {
			BRDF = pbrBrdf(normalize(-rayDirection), L, shadingNormal, material, randomStream);
		} else if (material->type == Diffuse)
		{
			BRDF = diffuseColour(material, vertices, uv, textures) / PI;
		}

		float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
		solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
		float3 Ld = scene->numEmissiveTriangles * lightColour * BRDF * dot(realNormal, L);
		outShadowData->flags = 0;
		outShadowData->multiplier = Ld * inData->multiplier;
		outShadowData->ray = createRay(intersection + L * EPSILON, L);
		outShadowData->rayLength = dist - 2 * EPSILON;
	}
	else {
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
	}


	float PDF;
	float3 reflection;
	if (material->type == PBR) {
		float choiceValue = clrngLfsr113RandomU01(randomStream);
		float3 f0;
		if (material->pbr.metallic) {
			f0 = material->pbr.reflectance;
		} else {
			f0 = material->pbr.f0NonMetal;
		}
		float f90 = 1.0f;
		reflection = ggxWeightedImportanceDirection(edge1, edge2, rayDirection, invTransform, 1 - material->pbr.smoothness, randomStream, &PDF);
		float LdotH = saturate(dot(-rayDirection, realNormal));
		float3 F = F_Schlick(f0, f90, LdotH);
		if (!material->pbr.metallic && choiceValue > F.x) {
			reflection = cosineWeightedDiffuseReflection(edge1, edge2, invTransform, randomStream);
			PDF = dot(realNormal, reflection) / PI;
		}
		BRDF = pbrBrdfChoice(normalize(-rayDirection), reflection, shadingNormal, material, choiceValue);
	}
	else if (material->type == Diffuse) {
		reflection = cosineWeightedDiffuseReflection(edge1, edge2, invTransform, randomStream);
		PDF = dot(realNormal, reflection) / PI;
		BRDF = diffuseColour(material, vertices, uv, textures) / PI;
	}

	if (dot(realNormal, reflection) < 0.0f)
	{
		// Stop if we have a ray going inside
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	// Continue random walk
	outData->flags = 0;
	float3 integral = BRDF * dot(realNormal, reflection) / PDF;
	outData->ray.origin = intersection + reflection * EPSILON;
	outData->ray.direction = reflection;
	outData->multiplier = inData->multiplier * integral;
	return BLACK;
}



// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 26
float3 neeShading(
	const __local Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const __global float* invTransform,
	float2 uv,
	image2d_array_t textures,
	clrngLfsr113Stream* randomStream,
	const __global RayData* inData,
	RayData* outData,
	RayData* outShadowData)
{
	// Gather intersection data
	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float3 edge1 = vertices[1].vertex - vertices[0].vertex;
	float3 edge2 = vertices[2].vertex - vertices[0].vertex;
	float3 realNormal = cross(edge1, edge2);
	realNormal = normalize(matrixMultiplyTranspose(invTransform, realNormal));
	float3 shadingNormal = interpolateNormal(vertices, uv);
	//realNormal = shadingNormal;
	
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

	float3 BRDF = 0;
	if (material->type == Diffuse) {
		BRDF = diffuseColour(material, vertices, uv, textures) * INVPI;
	}

	if (dot(realNormal, L) > 0.0f && dot(lightNormal, -L) > 0.0f)
	{
		if (material->type == PBR) {
			BRDF = pbrBrdf(normalize(-rayDirection), L, shadingNormal, material, randomStream);
		}

		float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
		solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
		float3 Ld = scene->numEmissiveTriangles * lightColour * solidAngle * BRDF * dot(realNormal, L);
		outShadowData->flags = 0;
		outShadowData->multiplier = Ld * inData->multiplier;
		outShadowData->ray = createRay(intersection + L * EPSILON, L);
		outShadowData->rayLength = dist - 2 * EPSILON;
	} else {
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
	}

	// Continue random walk
	float3 reflection = diffuseReflection(edge1, edge2, invTransform, randomStream);
	
	if (material->type == PBR) {
		BRDF = pbrBrdf(normalize(-rayDirection), reflection, shadingNormal, material, randomStream);
	}

	outData->flags = 0;
	float3 Ei = PI * 2.0f * dot(realNormal, reflection);
	float3 integral = BRDF * Ei;
	outData->ray.origin = intersection + reflection * EPSILON;
	outData->ray.direction = reflection;
	outData->multiplier = inData->multiplier * integral;
	return BLACK;
}



float3 naiveShading(
	const __local Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	const __global float* invTransform,
	float2 uv,
	image2d_array_t textures,
	clrngLfsr113Stream* randomStream,
	const __global RayData* inData,
	RayData* outData,
	RayData* outShadowData)
{
	outShadowData->flags = SHADINGFLAGS_HASFINISHED;
	// Gather intersection data
	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float3 edge1 = vertices[1].vertex - vertices[0].vertex;
	float3 edge2 = vertices[2].vertex - vertices[0].vertex;
	float3 realNormal = normalize(cross(edge1, edge2));
	realNormal = normalize(matrixMultiplyTranspose(invTransform, realNormal));
	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];

	if (dot(realNormal, -rayDirection) < 0.0f)
	{
		outData->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	// Terminate if we hit a light source
	if (material->type == Emissive)
	{
		outData->flags = SHADINGFLAGS_HASFINISHED;
		return inData->multiplier * material->emissive.emissiveColour;
	}

	// Continue in random direction
	float3 reflection = diffuseReflection(edge1, edge2, invTransform, randomStream);

	// Update throughput
	outData->flags = 0;
	//float3 BRDF = material->diffuse.diffuseColour / PI;
	float3 BRDF;
	if (material->type == PBR) {
		BRDF = pbrBrdf(normalize(-rayDirection), reflection, realNormal, material, randomStream);
	} else if (material->type == Diffuse)
	{
		BRDF = diffuseColour(material, vertices, uv, textures) / PI;
	}
	float3 Ei = dot(realNormal , reflection);// Irradiance
	float3 integral = PI * 2.0f * BRDF * Ei;
	float3 oldMultiplier = inData->multiplier;
	outData->ray.origin = intersection + reflection * EPSILON;
	outData->ray.direction = reflection;
	outData->multiplier = oldMultiplier * integral;
	return BLACK;
}

#endif// __SHADING_CL