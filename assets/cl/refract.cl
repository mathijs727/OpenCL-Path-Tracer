#ifndef __REFRACT_CL
#define __REFRACT_CL
#include "pbr_brdf.cl"

/*float3 refractiveBRDF(
	float3 V,
	float3 L,
	float3 N,
	const __global Material* material)
{
	float f0 = material->refractive.f0;
	float f90 = 1.0f;
	float roughness = 1.0f - material->refractive.smoothness;

	float NdotV = fabs(dot(N, V)) + 1e-5f; // avoid artifact
	float3 H = normalize(V + L);
	float LdotH = saturate(dot(L, H));
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));

	// Specular BRDF
	float3 F = F_Schlick(f0, f90, LdotH);
	float G = G_SmithGGXCorrelated(NdotL, NdotV, roughness);
	float D = D_GGX(NdotH, roughness);
	// The G function might return 0.0f because of the heavyside function.
	// Since OpenCL math is not that strict (and we dont want it to be for performance reasons),
	//  multiplying by 0.0f (which G may return) does not mean that Fr will actually become 0.0f.
	float3 Fr;
	if (G != 0.0f)
	{
		return F * D * G / (4.0f * NdotL * NdotV);
	}
	else {
		return BLACK;
	} 
}

float3 refractiveBTDF(
	float3 V,
	float3 L,
	float3 N,
	const __global Material* material)
{
	float f90 = 1.0f;
	float roughness = 1.0f - material->refractive.smoothness;

	float NdotV = fabs(dot(N, V)) + 1e-5f; // avoid artifact
	float3 H = normalize(V + L);
	float LdotH = saturate(dot(L, H));
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));

	float refracIndexOutside;
	float refracIndexInside;
	if (dot(N, V) > 0)
	{
		refracIndexOutside = 1.000277f;
		refracIndexInside = material->refractive.refractiveIndex;
	} else {
		refracIndexOutside = material->refractive.refractiveIndex;
		refracIndexInside = 1.000277f;
	}
	float refracIndexOutside2 = refracIndexOutside * refracIndexOutside;

	// Specular BRDF
	float3 F = F_Schlick(f0, f90, LdotH);
	float G = G_SmithGGXCorrelated(NdotL, NdotV, roughness);
	float D = D_GGX(NdotH, roughness);

	// Refraction term according to equation 21 of:
	// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
	float frac1 = (dot(L, H) * dot(V, H)) / (dot(L, N) * dot(V, N));
	float3 frac2Top = refracIndexOutside2 * (1 - F) * G * D;
	float frac2BotSqrt = (refracIndexInside * dot(L, H) + refracIndexOutside * dot(V, H));
	float frac2Bot = frac2BotSqrt * frac2BotSqrt;
	float3 f_t = frac1 * (frac2Top / frac2Bot);
	return f_t;
}


// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
float3 refractiveBSDF(
	float3 V, 
	float3 L,
	float3 N,
	const __global Material* material)
{
	/if (dot(N, V) > 0.0f)
	{
		// From outside
		if (dot(N, L) > 0.0f)
		{
			// To outside
			return refractiveBRDF(V, L, N, material);
		} else {
			// To inside
			return refractiveBTDF(V, L, N, material);
		}
	} else {
		// From inside
		if (dot(N, L) > 0.0f)
		{
			// To outside
			return refractiveBTDF(V, L, -N, material);
		} else {
			// To inside
			return refractiveBRDF(V, L, -N, material);
		}
	}/

	return BLACK;// Never reached
}*/

// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
// Section 5.3
float calcWeight(float3 I, float3 N, float3 M, const __global Material* material, float3 O)
{
	float IdotM = fabs(dot(I, M));
	float MdotN = fabs(dot(M, N));
	float NdotI = fabs(dot(N, I));
	float NdotO = fabs(dot(N, O));

	float roughness = 1.0f - material->refractive.smoothness;
	float G = G_SmithGGXCorrelated(NdotI, NdotO, roughness);
	float denom = NdotI * MdotN;
	//if (denom > EPSILON) {
		return (IdotM * G) / denom;
	//}	
	//else return 1000000.0f; // very high value
}

// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
// Section 5.3
float evaluateReflect(float3 I, float3 N, float3 M, const __global Material* material, float3* o_r)
{
	*o_r = normalize(I - 2 * dot(I, M) * M);
	return calcWeight(I, N, M, material, *o_r);
}

// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
// Section 5.3
// n_i / n_t = refractive indices
float evaluateRefract(float3 I, float3 N, float3 M, float n1n2, float IdotM, float K, const __global Material* material, float3* o_t)
{
	//float c = dot(I, M);
	//float n = n_i / n_t;

	*o_t = normalize(-n1n2 * I + M * (n1n2 * IdotM - sqrt(K))); //normalize((n * c - sign(dot(I, N)) * sqrt(1 + n * (c*c - 1)) ) * M - n * I);
	
	return calcWeight(I, N, M, material, *o_t);
}

#endif// __REFRACT_CL