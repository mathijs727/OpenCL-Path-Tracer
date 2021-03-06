#ifndef __SHADING_CL
#define __SHADING_CL
#include "shading_helper.cl"
#include "random.cl"
#include "light.cl"
#include "pbr_brdf.cl"
#include "refract.cl"

#define MAXSMOOTHNESS 0.94f

enum {
	SHADINGFLAGS_HASFINISHED = 1,
	SHADINGFLAGS_LASTSPECULAR = 2
};

typedef struct {
	Ray ray;// 2 * float3 = 32 bytes
	float3 multiplier;// 16 bytes
	size_t outputPixel;// 4 bytes?
	int flags;// 4 bytes
	union// 4 bytes
	{
		float rayLength;// Only shadows use this
		int numBounces;// And shadows dont bounce
	};
	float pdf; // 4 bytes
	float t;// 4 bytes
	// Aligned to 16 bytes so struct has size of 80 bytes
} RayData;



// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 49/51
float3 neeMisShading(// Next Event Estimation + Multiple Importance Sampling
	const __local Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	float t,
	const __global float* invTransform,
	float2 uv,
	image2d_array_t textures,
	randStream* randomStream,
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
	float3 raySideNormal = shadingNormal;
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
			// Do MIS
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
			else return BLACK;
			float pdf2 = inData->pdf;
			if (pdf2 < EPSILON) return BLACK;
			float weight = pdf2 / (pdf1 + pdf2);
			return inData->multiplier * material->emissive.emissiveColour * weight;
		}
	}
	
	float3 BRDF = 0.0f;
	if (material->type == REFRACTIVE || material->type == BASIC_REFRACTIVE)
	{
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
	}
	else
	{
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
		if (dot(shadingNormal, L) > EPSILON && dot(realNormal, L) > EPSILON && dot(lightNormal, -L) > EPSILON)// dot(realNormal, L) may be <0.0f for transparent materials?
		{
			// do MIS
			float pdf2 = 0.0f;
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
				float3 halfway = normalize(-rayDirection + L);
				float LdotH = saturate(dot(L, halfway));
				float3 F = F_Schlick(f0, f90, LdotH);
				float rand01 = randRandomU01(randomStream);
				float NdotH = dot(shadingNormal, halfway);
				if (!material->pbr.metallic && rand01 > F.x) {
					pdf2 = dot(shadingNormal, L) / PI; // cosine weighted PDF
				}
				else {
					pdf2 = D_GGX(NdotH, 1.0f - material->pbr.smoothness);
				}

				float3 c = diffuseColour(material, vertices, uv, textures);
				if (c.x == -1.0f) {
					BRDF = 0.0f;
				} else {
					BRDF = c / PI;
				}
			}
			else if (material->type == DIFFUSE)
			{
				BRDF = diffuseColour(material, vertices, uv, textures) / PI;
				pdf2 = dot(realNormal, L) / PI;
			}
			float solidAngle = 2*PI;
			if (dist2 > EPSILON)
			{
				solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
				solidAngle = clamp(solidAngle, 0.0f, 2*PI);// Prevents white dots when dist is really small
			}
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
	}

	float dospecular = false;
	float PDF;
	outData->pdf = 0; // for materials not interacting with MIS
	float cosineTerm;
	float3 reflection;
	if (material->type == PBR) {
		float3 f0;
		if (material->pbr.metallic) {
			f0 = material->pbr.reflectance;
		} else {
			f0 = material->pbr.f0NonMetal;
		}
		float roughness = 1.0f - material->pbr.smoothness;
		float3 V = -rayDirection;
		float f90 = 1.0f;
		float3 halfway;
		reflection = ggxWeightedImportanceDirection(shadingNormal, rayDirection, invTransform, 1 - material->pbr.smoothness, randomStream, &PDF, &halfway);
		cosineTerm = dot(shadingNormal, reflection);
		if (cosineTerm < 0.05f || dot(realNormal, reflection) < EPSILON) {
			outData->flags = SHADINGFLAGS_HASFINISHED;
			return BLACK;
		}
		float LdotH = saturate(dot(reflection, halfway));
		float3 F = F_Schlick(f0, f90, LdotH);
		float rand01 = randRandomU01(randomStream);
		if (!material->pbr.metallic && rand01 > F.x)
		{
			reflection = cosineWeightedDiffuseReflection(shadingNormal, edge1, invTransform, randomStream);
			//cosineTerm = dot(reflection, shadingNormal);// Already set (line 333)
			PDF = INVPI; // cosine simplification
			cosineTerm = 1.0f;
			outData->pdf = dot(shadingNormal, reflection) * INVPI; // MIS need the real unsimplified PDF
			BRDF = diffuseOnly(V, halfway, reflection, shadingNormal, material);
		} else {
			//cosineTerm = dot(reflection, shadingNormal);// Already set (line 333)
			//PDF = ...GGX_NDF...;// PDF set by the ggx sampling function (line 332)
			outData->pdf = PDF; // MIS needs real PDF
			// This not only saves operations, but more importantly, it reduces floating point arithmatic errors.
			PDF = 1.0f;// We set the PDF to 1 so that we do not have to calculate the NDF in the BRDF function
			BRDF = brdfOnlyNoFresnelNoNDF(V, halfway, reflection, shadingNormal, material);
			if (material->pbr.metallic) 
			{	
				// since we are sampling the reflections every time for metallics, 
				// they're not weighted based on the sampling rate. We need to multiply by the fresnel term
				BRDF *= F;
			}
			if (material->pbr.smoothness > MAXSMOOTHNESS) dospecular = true;
		}		
	} else if (material->type == BASIC_REFRACTIVE)
	{
		// Slide 34
		// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2001%20-%20intro%20&%20whitted.pdf
		float3 D = rayDirection;
		float3 absorptionFactor = 1.0f;

		float n1, n2;
		if (dot(realNormal, -D) > EPSILON)
		{
			n1 = 1.000277f;
			n2 = material->basicRefractive.refractiveIndex;
		} else {
			n1 = material->basicRefractive.refractiveIndex;
			n2 = 1.000277f;
			absorptionFactor = exp(-material->basicRefractive.absorption * t);
		}
		float cos1 = dot(raySideNormal, -D);
		float n1n2 = n1 / n2;
		float K = 1 - (n1n2*n1n2) * (1 - cos1*cos1);
		if (K > EPSILON)
		{
			float rand01 = randRandomU01(randomStream);
			float f0 = pow((n1-n2)/(n1+n2),2.0f);
			float3 F = F_Schlick(f0, 1.0f, dot(raySideNormal, -rayDirection));
			if (rand01 < F.x) {
				// Reflection
				reflection = normalize(-D - 2 * dot(-D, raySideNormal) * raySideNormal);
			}
			else {
				// Refraction
				reflection = normalize(n1n2 * D + raySideNormal * (n1n2 * cos1 - sqrt(K)));
			}
		} else {
			// Total internal reflection
			reflection = normalize(-D - 2 * dot(-D, raySideNormal) * raySideNormal);
		}

		// Apply beer's law by multiplying it with the BRDF
		BRDF = absorptionFactor;
		cosineTerm = 1.0f;
		PDF = 1.0f;
	} else if (material->type == REFRACTIVE)
	{
		float3 halfway = beckmannWeightedHalfway(raySideNormal, rayDirection, invTransform, 1 - material->refractive.smoothness, randomStream);
		//if (dot(halfway, raySideNormal) < 0.9f) {
		//	outData->flags = SHADINGFLAGS_HASFINISHED;
		//	return BLACK;
		//}
		float3 absorptionFactor = 1.0f;

		float n_i, n_t;
		if (dot(realNormal, -rayDirection) > 0.0f)
		{
			// Hit from outside, refracting inwards
			n_i = 1.000277f;
			n_t = material->refractive.refractiveIndex;			
		} else {
			// Hit from inside, refracting outwards
			n_i = material->refractive.refractiveIndex;
			n_t = 1.000277f;

			// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2001%20-%20intro%20&%20whitted.pdf
			// Slide 42
			absorptionFactor = exp(-material->refractive.absorption * t);
		}

		float f0 = pow((n_i - n_t) / (n_i + n_t), 2);
		float f90 = 1.0f;
		float3 F = F_Schlick(f0, f90, dot(-rayDirection, halfway));
		float rand01 = randRandomU01(randomStream);
		if (rand01 < F.x)
		{
			BRDF = evaluateReflect(-rayDirection, raySideNormal, halfway, material, &reflection);
		} else {
			float n1n2 = n_i / n_t;
			float cos1 = dot(halfway, -rayDirection);
			float K = 1 - (n1n2*n1n2) * (1 - cos1*cos1);
			if (K >= 0) {
				BRDF = evaluateRefract(-rayDirection, raySideNormal, halfway, n1n2, cos1, K, material, &reflection);
			} else {
				BRDF = evaluateReflect(-rayDirection, raySideNormal, halfway, material, &reflection);
			}
		}
		// Apply beer's law by multiplying it with the BRDF
		BRDF *= absorptionFactor;

		// Remove the cos from the integral, because its integrated in the BRDF
		cosineTerm = 1.0f;
		PDF = 1.0f;
	} else if (material->type == DIFFUSE) {
		cosineTerm = 1.0f;
		
		float3 c = diffuseColour(material, vertices, uv, textures);
		if (c.x == -1.0f)// Transparent
		{
			PDF = 1.0f;
			reflection = rayDirection;
			BRDF = 1.0f;
		} else {
			PDF = 1.0f; // we simplify the cosine term away from PDF and cosineTerm
			outData->pdf = dot(shadingNormal, reflection) * INVPI; // MIS needs unsimplified PDF
			reflection = cosineWeightedDiffuseReflection(realNormal, edge1, invTransform, randomStream);
			BRDF = c;
		}
	}

	//PDF = fmax(PDF, 0.25f);
	//probabilityToSurvive = fmax(probabilityToSurvive, 0.25f);
	// Continue random walk
	//BRDF = 0.1f;
	outData->flags = 0;
	if (material->type == REFRACTIVE || material->type == BASIC_REFRACTIVE || dospecular)
		outData->flags = SHADINGFLAGS_LASTSPECULAR;
	float3 integral = BRDF * cosineTerm / PDF;

	float probabilityToSurvive = fmax(fmax(integral.x, integral.y), integral.z);
	probabilityToSurvive = saturate(probabilityToSurvive);
	float choiceToSurvive = randRandomU01(randomStream);
	if (probabilityToSurvive < EPSILON || choiceToSurvive > probabilityToSurvive) {
		outData->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	outData->ray.origin = intersection + reflection * EPSILON;
	outData->ray.direction = reflection;
	outData->multiplier = inData->multiplier * integral / probabilityToSurvive;
	return BLACK;
}




// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2008%20-%20variance%20reduction.pdf
// Slide 42
float3 neeIsShading(// Next Event Estimation + Importance Sampling
	const __local Scene* scene,
	int triangleIndex,
	float3 intersection,
	float3 rayDirection,
	float t,
	const __global float* invTransform,
	float2 uv,
	image2d_array_t textures,
	randStream* randomStream,
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
	float3 raySideNormal = shadingNormal;
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
	
	float3 BRDF = 0.0f;
	if (material->type == REFRACTIVE || material->type == BASIC_REFRACTIVE)
	{
		outShadowData->flags = SHADINGFLAGS_HASFINISHED;
	}
	else
	{
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
		if (dot(shadingNormal, L) > EPSILON && dot(realNormal, L) > EPSILON && dot(lightNormal, -L) > EPSILON)// dot(realNormal, L) may be <0.0f for transparent materials?
		{
			if (material->type == PBR) {
				BRDF = pbrBrdfWithDiffuse(-rayDirection, L, shadingNormal, material, material->pbr.smoothness > MAXSMOOTHNESS);
			} else if (material->type == DIFFUSE)
			{

				float3 c = diffuseColour(material, vertices, uv, textures);
				if (c.x == -1.0f) {
					BRDF = 0.0f;
				} else {
					BRDF = c / PI;
				}
			}
			float solidAngle = 2*PI;
			if (dist2 > EPSILON)
			{
				solidAngle = (dot(lightNormal, -L) * lightArea) / dist2;
				solidAngle = clamp(solidAngle, 0.0f, 2*PI);// Prevents white dots when dist is really small
			}
			float3 Ld = scene->numEmissiveTriangles * lightColour * BRDF * solidAngle * dot(shadingNormal, L);
			outShadowData->flags = 0;
			outShadowData->multiplier = Ld * inData->multiplier;
			outShadowData->ray = createRay(intersection + L * EPSILON, L);
			outShadowData->rayLength = dist - 2 * EPSILON;
		}
		else {
			outShadowData->flags = SHADINGFLAGS_HASFINISHED;
		}
	}



	float dospecular = false;
	float PDF;
	float cosineTerm;
	float3 reflection;
	if (material->type == PBR) {
		float3 f0;
		if (material->pbr.metallic) {
			f0 = material->pbr.reflectance;
		} else {
			f0 = material->pbr.f0NonMetal;
		}
		float roughness = 1.0f - material->pbr.smoothness;
		float3 V = -rayDirection;
		float f90 = 1.0f;
		float3 halfway;
		reflection = ggxWeightedImportanceDirection(shadingNormal, rayDirection, invTransform, 1 - material->pbr.smoothness, randomStream, &PDF, &halfway);
		cosineTerm = dot(shadingNormal, reflection);
		if (cosineTerm < 0.05f || dot(realNormal, reflection) < EPSILON) {
			outData->flags = SHADINGFLAGS_HASFINISHED;
			return BLACK;
		}
		float LdotH = saturate(dot(reflection, halfway));
		float3 F = F_Schlick(f0, f90, LdotH);
		float rand01 = randRandomU01(randomStream);
		if (!material->pbr.metallic && rand01 > F.x)
		{
			reflection = cosineWeightedDiffuseReflection(shadingNormal, edge1, invTransform, randomStream);
			//cosineTerm = dot(reflection, shadingNormal);// Already set (line 333)
			PDF = INVPI; // cosine simplification
			cosineTerm = 1.0f;
			BRDF = diffuseOnly(V, halfway, reflection, shadingNormal, material);
		} else {
			//cosineTerm = dot(reflection, shadingNormal);// Already set (line 333)
			//PDF = ...GGX_NDF...;// PDF set by the ggx sampling function (line 332)
			PDF = 1.0f;// We set the PDF to 1 so that we do not have to calculate the NDF in the BRDF function
			// This not only saves operations, but more importantly, it reduces floating point arithmatic errors.
			BRDF = brdfOnlyNoFresnelNoNDF(V, halfway, reflection, shadingNormal, material);
			if (material->pbr.metallic) 
			{	
				// since we are sampling the reflections every time for metallics, 
				// they're not weighted based on the sampling rate. We need to multiply by the fresnel term
				BRDF *= F;
			}
			if (material->pbr.smoothness > MAXSMOOTHNESS) dospecular = true;
		}		
	} else if (material->type == BASIC_REFRACTIVE)
	{
		// Slide 34
		// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2001%20-%20intro%20&%20whitted.pdf
		float3 D = rayDirection;
		float3 absorptionFactor = 1.0f;

		float n1, n2;
		if (dot(realNormal, -D) > EPSILON)
		{
			n1 = 1.000277f;
			n2 = material->basicRefractive.refractiveIndex;
		} else {
			n1 = material->basicRefractive.refractiveIndex;
			n2 = 1.000277f;
			absorptionFactor = exp(-material->basicRefractive.absorption * t);
		}
		float cos1 = dot(raySideNormal, -D);
		float n1n2 = n1 / n2;
		float K = 1 - (n1n2*n1n2) * (1 - cos1*cos1);
		if (K > EPSILON)
		{
			float rand01 = randRandomU01(randomStream);
			float f0 = pow((n1-n2)/(n1+n2),2.0f);
			float3 F = F_Schlick(f0, 1.0f, dot(raySideNormal, -rayDirection));
			if (rand01 < F.x) {
				// Reflection
				reflection = normalize(-D - 2 * dot(-D, raySideNormal) * raySideNormal);
			}
			else {
				// Refraction
				reflection = normalize(n1n2 * D + raySideNormal * (n1n2 * cos1 - sqrt(K)));
			}
		} else {
			// Total internal reflection
			reflection = normalize(-D - 2 * dot(-D, raySideNormal) * raySideNormal);
		}

		// Apply beer's law by multiplying it with the BRDF
		BRDF = absorptionFactor;
		cosineTerm = 1.0f;
		PDF = 1.0f;
	} else if (material->type == REFRACTIVE)
	{
		float3 halfway = beckmannWeightedHalfway(raySideNormal, rayDirection, invTransform, 1 - material->refractive.smoothness, randomStream);
		//if (dot(halfway, raySideNormal) < 0.9f) {
		//	outData->flags = SHADINGFLAGS_HASFINISHED;
		//	return BLACK;
		//}
		float3 absorptionFactor = 1.0f;

		float n_i, n_t;
		if (dot(realNormal, -rayDirection) > 0.0f)
		{
			// Hit from outside, refracting inwards
			n_i = 1.000277f;
			n_t = material->refractive.refractiveIndex;			
		} else {
			// Hit from inside, refracting outwards
			n_i = material->refractive.refractiveIndex;
			n_t = 1.000277f;

			// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2001%20-%20intro%20&%20whitted.pdf
			// Slide 42
			absorptionFactor = exp(-material->refractive.absorption * t);
		}

		float f0 = pow((n_i - n_t) / (n_i + n_t), 2);
		float f90 = 1.0f;
		float3 F = F_Schlick(f0, f90, dot(-rayDirection, halfway));
		float rand01 = randRandomU01(randomStream);
		if (rand01 < F.x)
		{
			BRDF = evaluateReflect(-rayDirection, raySideNormal, halfway, material, &reflection);
		} else {
			float n1n2 = n_i / n_t;
			float cos1 = dot(halfway, -rayDirection);
			float K = 1 - (n1n2*n1n2) * (1 - cos1*cos1);
			if (K >= 0) {
				BRDF = evaluateRefract(-rayDirection, raySideNormal, halfway, n1n2, cos1, K, material, &reflection);
			} else {
				BRDF = evaluateReflect(-rayDirection, raySideNormal, halfway, material, &reflection);
			}
		}
		// Apply beer's law by multiplying it with the BRDF
		BRDF *= absorptionFactor;

		// Remove the cos from the integral, because its integrated in the BRDF
		cosineTerm = 1.0f;
		PDF = 1.0f;
	} else if (material->type == DIFFUSE) {
		cosineTerm = 1.0f;

		float3 c = diffuseColour(material, vertices, uv, textures);
		if (c.x == -1.0f)// Transparent
		{
			PDF = 1.0f;
			reflection = rayDirection;
			BRDF = 1.0f;
		} else {
			PDF = 1.0f; // we simplify the cosine term away from PDF and cosineTerm
			reflection = cosineWeightedDiffuseReflection(realNormal, edge1, invTransform, randomStream);
			BRDF = c;
		}
	}
	//PDF = fmax(PDF, 0.25f);
	//probabilityToSurvive = fmax(probabilityToSurvive, 0.25f);
	// Continue random walk
	//BRDF = 0.1f;
	outData->flags = 0;
	if (material->type == REFRACTIVE || material->type == BASIC_REFRACTIVE || dospecular)
		outData->flags = SHADINGFLAGS_LASTSPECULAR;
	float3 integral = BRDF * cosineTerm / PDF;

	float probabilityToSurvive = fmax(fmax(integral.x, integral.y), integral.z);
	probabilityToSurvive = saturate(probabilityToSurvive);
	float choiceToSurvive = randRandomU01(randomStream);
	if (probabilityToSurvive < EPSILON || choiceToSurvive > probabilityToSurvive) {
		outData->flags = SHADINGFLAGS_HASFINISHED;
		return BLACK;
	}

	outData->ray.origin = intersection + reflection * EPSILON;
	outData->ray.direction = reflection;
	outData->multiplier = inData->multiplier * integral / probabilityToSurvive;
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
	randStream* randomStream,
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
	randStream* randomStream,
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