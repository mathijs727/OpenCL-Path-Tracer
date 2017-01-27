#ifndef __SHADING_CL
#define __SHADING_CL
#include "shading_helper.cl"
#include <clRNG/lfsr113.clh>
#include "light.cl"
#include "pbr_brdf.cl"
#include "refract.cl"

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
	float pdf; // 4
	// Aligned to 16 bytes so struct has size of 80 bytes
} RayData;


/*float3 v[3];
for (int i = 0; i < 3; ++i) v[i] = vertices[i].vertex;
float lightArea = triangleArea(v);
float3 distV = intersection - inData->ray.origin;
float dist2 = dot(distV, distV);
float solidAngle = (dot(realNormal, -rayDirection) * lightArea) / dist2;
solidAngle = min(2 * PI, solidAngle);
float pdf1 = 1 / solidAngle;
float pdf2 = inData->pdf;
float weight = pdf2 / (pdf1 + pdf2);
return inData->multiplier * material->emissive.emissiveColour * weight;*/


/*float3 f0;
if (material->pbr.metallic) {
f0 = material->pbr.reflectance;
}
else {
f0 = material->pbr.f0NonMetal;
}
float f90 = 1.0f;
float3 H = normalize(-rayDirection + L);
float LdotH = saturate(dot(L, H));
float3 F = F_Schlick(f0, f90, LdotH);
float rand01 = clrngLfsr113RandomU01(randomStream);
if (material->pbr.metallic || rand01 < F.x) {
float a = 1 - material->pbr.smoothness;
float dotProduct = dot(H, realNormal);
float denom = dotProduct*dotProduct*(a*a - 1) + 1;
pdf2 = a*a / (PI*denom*denom);
}
else {
pdf2 = dot(realNormal, L) / PI;
}			*/

//float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
//solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
//float pdf1 = 0;
//if (solidAngle > EPSILON) {
//	pdf1 = 1 / solidAngle;
//}


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

	if (dot(realNormal, -rayDirection) < 0.0f)
	{
		// Stop if we hit the back side
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	// Terminate if we hit a light source
	if (material->type == EMISSIVE)
	{
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		if (inData->flags & SHADINGFLAGS_LASTSPECULAR) {
			return inData->multiplier * material->emissive.emissiveColour;
		}
		else {
			float3 v[3];
			for (int i = 0; i < 3; ++i) v[i] = vertices[i].vertex;
			float lightArea = triangleArea(v);
			float3 distV = intersection - inData->ray.origin;
			float dist2 = dot(distV, distV);
			float solidAngle = (dot(realNormal, -rayDirection) * lightArea) / dist2;
			solidAngle = min(2 * PI, solidAngle);
			float pdf1 = 0;
			if (solidAngle > EPSILON) {
				pdf1 = 1 / solidAngle;
			}
			float pdf2 = inData->pdf;
			float weight = pdf2 / (pdf1 + pdf2);
			return inData->multiplier * material->emissive.emissiveColour * weight;
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

	float3 BRDF = 0.0f;
	if (dot(realNormal, L) > 0.0f && dot(lightNormal, -L) > 0.0f)
	{
		float pdf2;
		if (material->type == PBR) {
			BRDF = pbrBrdf(normalize(-rayDirection), L, shadingNormal, material);
			float3 f0;
			if (material->pbr.metallic) {
				f0 = material->pbr.reflectance;
			}
			else {
				f0 = material->pbr.f0NonMetal;
			}
			float f90 = 1.0f;
			float3 H = normalize(-rayDirection + L);
			float LdotH = saturate(dot(L, H));
			float3 F = F_Schlick(f0, f90, LdotH);
			float rand01 = clrngLfsr113RandomU01(randomStream);
				if (material->pbr.metallic || rand01 < F.x) {
				float a = 1 - material->pbr.smoothness;
				float NdotH = dot(H, realNormal);
				pdf2 = 0;
				if (NdotH > EPSILON) {
					float denom = NdotH*NdotH*(a*a - 1) + 1;
					pdf2 = a*a / (PI*denom*denom);
				}
			}
			else {
				pdf2 = dot(realNormal, L) / PI;
			}
		}
		else if (material->type == DIFFUSE)
		{
			BRDF = diffuseColour(material, vertices, uv, textures) / PI;
			pdf2 = dot(realNormal, L) / PI;
		}

		float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
		solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
		float pdf1 = 1 / solidAngle;

		//float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
		//solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
		float3 Ld = scene->numEmissiveTriangles * lightColour * BRDF * dot(realNormal, L) / (pdf1+pdf2);
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
		float3 f0;
		if (material->pbr.metallic) {
			f0 = material->pbr.reflectance;
		}
		else {
			f0 = material->pbr.f0NonMetal;
		}
		float3 V = normalize(-rayDirection);
		float f90 = 1.0f;
		reflection = ggxWeightedImportanceDirection(edge1, edge2, rayDirection, invTransform, 1 - material->pbr.smoothness, randomStream, &PDF);
		float3 H = normalize(V + reflection);
		float LdotH = saturate(dot(reflection, H));
		float3 F = F_Schlick(f0, f90, LdotH);
		float rand01 = clrngLfsr113RandomU01(randomStream);
		if (!material->pbr.metallic && rand01 > F.x)
		{
			reflection = cosineWeightedDiffuseReflection(edge1, edge2, invTransform, randomStream);
			PDF = dot(realNormal, reflection) / PI;
			BRDF = diffuseOnly(V, reflection, shadingNormal, material);
		}
		else {
			BRDF = brdfOnly(V, reflection, shadingNormal, material);
		}


		/*reflection = ggxWeightedImportanceDirection(edge1, edge2, rayDirection, invTransform, 1 - material->pbr.smoothness, randomStream, &PDF);
		float LdotH = saturate(dot(-rayDirection, realNormal));
		float3 F = F_Schlick(f0, f90, LdotH);
		if (!material->pbr.metallic && choiceValue > F.x) {
		reflection = cosineWeightedDiffuseReflection(edge1, edge2, invTransform, randomStream);
		PDF = dot(realNormal, reflection) / PI;
		}
		BRDF = pbrBrdfChoice(normalize(-rayDirection), reflection, shadingNormal, material, choiceValue);*/
	}
	else if (material->type == DIFFUSE) {
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
	outData->pdf = PDF;
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
	float3 shadingNormal = realNormal;//interpolateNormal(vertices, uv);
	float3 raySideNormal = realNormal;
	if (dot(raySideNormal, -rayDirection) < 0.0f)
		raySideNormal *= -1;
	//shadingNormal = realNormal;

	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];

	// Terminate if we hit a light source
	if (material->type == EMISSIVE)
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
		/*// Stop if we hit the back side
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		return WHITE;*/
		//realNormal *= -1;
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
	if (dot(realNormal, L) > 0.0f && dot(lightNormal, -L) > 0.0f)// dot(realNormal, L) may be <0.0f for transparent materials?
	{
		if (material->type == PBR) {
			BRDF = pbrBrdf(-rayDirection, L, shadingNormal, material);
		} else if (material->type == DIFFUSE)
		{
			BRDF = diffuseColour(material, vertices, uv, textures) / PI;
		} else if (material->type == REFRACTIVE || material->type == BASIC_REFRACTIVE)
		{
			//BRDF = refractiveBSDF(-rayDirection, L, shadingNormal, material);
			BRDF = 0;
		}

		//float solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
		//solidAngle = min(2 * PI, solidAngle);// Prevents white dots when dist is really small
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
		float3 f0;
		if (material->pbr.metallic) {
			f0 = material->pbr.reflectance;
		} else {
			f0 = material->pbr.f0NonMetal;
		}
		float3 V = -rayDirection;
		float f90 = 1.0f;
		reflection = ggxWeightedImportanceDirection(edge1, edge2, rayDirection, invTransform, 1 - material->pbr.smoothness, randomStream, &PDF);
		float3 H = normalize(V + reflection);
		float LdotH = saturate(dot(reflection, H));
		float3 F = F_Schlick(f0, f90, LdotH);
		float rand01 = clrngLfsr113RandomU01(randomStream);
		if (!material->pbr.metallic && rand01 > F.x)
		{
			reflection = cosineWeightedDiffuseReflection(edge1, edge2, invTransform, randomStream);
			PDF = dot(realNormal, reflection) / PI;
			BRDF = diffuseOnly(V, reflection, shadingNormal, material);
		} else {
			BRDF = brdfOnly(V, reflection, shadingNormal, material);
		}
	} else if (material->type == BASIC_REFRACTIVE)
	{
		// Slide 34
		// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2001%20-%20intro%20&%20whitted.pdf
		float3 D = rayDirection;

		float n1, n2;
		if (dot(realNormal, -D) > 0.0f)
		{
			n1 = 1.000277f;
			n2 = material->basicRefractive.refractiveIndex;
		} else {
			n1 = material->basicRefractive.refractiveIndex;
			n2 = 1.000277f;
		}
		float cos1 = dot(raySideNormal, -D);
		float n1n2 = n1 / n2;
		float K = 1 - (n1n2*n1n2) * (1 - cos1*cos1);
		if (K >= 0)
		{
			// Refraction
			reflection = normalize(D * n1n2 + realNormal * (n1n2 * cos1 - sqrt(K)));
		} else {
			// Total internal reflection
			reflection = normalize(-D - 2 * dot(-D, raySideNormal) * raySideNormal);
			//reflection = BLACK;
		}
		BRDF = 1.0f;
		PDF = 1.0f;
	} else if (material->type == REFRACTIVE)
	{
		/*if (dot(-rayDirection, realNormal) < 0.0f)
		{
			reflection = rayDirection;
			PDF = 1.0f;
			BRDF = 1.0f;
		} else */{
			float3 halfway = ggxWeightedHalfway(edge1, edge2, invTransform, 1 - material->refractive.smoothness, randomStream);
			if (dot(halfway, -rayDirection) < 0.0f)// We may hit the mesh at the inside
				halfway *= -1;// Where the normal (and thus halfway) has to point inward
			float f0 = material->refractive.f0;
			float f90 = 1.0f;
			float3 F = F_Schlick(f0, f90, dot(-rayDirection, halfway));
			float rand01 = clrngLfsr113RandomU01(randomStream);
			if (rand01 < F.x)
			{
				BRDF = evaluateReflect(-rayDirection, realNormal, halfway, material, &reflection);
			} else {
				float n1, n2;
				if (dot(realNormal, -rayDirection) >= 0.0f)
				{
					n1 = 1.000277f;
					n2 = material->refractive.refractiveIndex;
				} else {
					n1 = material->refractive.refractiveIndex;
					n2 = 1.000277f;
				}
				BRDF = evaluateRefract(-rayDirection, raySideNormal, halfway, n1, n2, material, &reflection);
			}
		}
		// Remove the cos from the integral, because its integrated in the BRDF
		PDF = dot(raySideNormal, reflection);
	} else if (material->type == DIFFUSE) {
		reflection = cosineWeightedDiffuseReflection(edge1, edge2, invTransform, randomStream);
		PDF = dot(realNormal, reflection) / PI;
		BRDF = diffuseColour(material, vertices, uv, textures) / PI;
	}

	/*if (dot(realNormal, reflection) < 0.0f)
	{
		// Stop if we have a ray going inside
		outData->flags = SHADINGFLAGS_HASFINISHED;
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}*/

	// Continue random walk
	outData->flags = 0;
	if (material->type == REFRACTIVE || material->type == BASIC_REFRACTIVE)
		outData->flags = SHADINGFLAGS_LASTSPECULAR;
	float3 integral = BRDF * dot(raySideNormal, reflection) / PDF;
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
	if (material->type == EMISSIVE)
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
	if (material->type == DIFFUSE) {
		BRDF = diffuseColour(material, vertices, uv, textures) * INVPI;
	}

	if (dot(realNormal, L) > 0.0f && dot(lightNormal, -L) > 0.0f)
	{
		if (material->type == PBR) {
			BRDF = pbrBrdf(normalize(-rayDirection), L, shadingNormal, material);
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
		BRDF = pbrBrdf(normalize(-rayDirection), reflection, shadingNormal, material);
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
	if (material->type == EMISSIVE)
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
		BRDF = pbrBrdf(normalize(-rayDirection), reflection, realNormal, material);
	} else if (material->type == DIFFUSE)
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