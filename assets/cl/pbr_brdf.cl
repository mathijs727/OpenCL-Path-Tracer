#ifndef __PBR_BRDF_CL
#define __PBR_BRDF_CL
#include "math.cl"

// Micro facet BRDF as described in the course notes of "Moving Frostbite to PBR":
// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf

// Very good presentation on this topic (Siggraph University):
// https://www.youtube.com/watch?v=j-A0mwsJRmk

// Diffuse:
// Normalized Disney

// Reflection:
// F * G * D / (4 * dot(N, V) * dot(N, L))
// F = Fresnel Law (Schlick approximation)
// G = Geometry factor (height correlated Smith)
// D = Normal distribution (GGX)

float3 F_Schlick(float3 f0, float f90, float u)// u = NdotL
{
	return f0 + (f90 - f0) * pow (1.0f - u, 5.0f);
}

float G_SmithGGXCorrelated(float NdotL, float NdotV, float alphaG)
{
	// Original formulation of G_SmithGGX Correlated
	// lambda_v = ( -1 + sqrt ( alphaG2 * (1 - NdotL2 ) / NdotL2 + 1)) * 0.5 f;
	// lambda_l = ( -1 + sqrt ( alphaG2 * (1 - NdotV2 ) / NdotV2 + 1)) * 0.5 f;
	// G_SmithGGXCorrelated = 1 / (1 + lambda_v + lambda_l );
	// V_SmithGGXCorrelated = G_SmithGGXCorrelated / (4.0 f * NdotL * NdotV );

	// This is the optimize version
	float alphaG2 = alphaG * alphaG;
	// Caution : the " NdotL *" and " NdotV *" are explicitely inversed , this is not a mistake .
	float Lambda_GGXV = NdotL * sqrt((-NdotV * alphaG2 + NdotV) * NdotV + alphaG2);
	float Lambda_GGXL = NdotV * sqrt((-NdotL * alphaG2 + NdotL) * NdotL + alphaG2);

	return 0.5f / (Lambda_GGXV + Lambda_GGXL);
}

float D_GGX (float NdotH, float m)
{
	// Divide by PI is apply later
	float m2 = m * m ;
	float f = (NdotH * m2 - NdotH) * NdotH + 1;
	return m2 / (f * f) ;
}

float Fr_DisneyDiffuse (float NdotV, float NdotL, float LdotH, float linearRoughness)
{
	float energyBias = lerp(0, 0.5f , linearRoughness );
	float energyFactor = lerp(1.0f , 1.0f / 1.51f , linearRoughness );
	float fd90 = energyBias + 2.0f * LdotH * LdotH * linearRoughness;
	float3 f0 = (float3)(1.0f, 1.0f, 1.0f);
	float lightScatter = F_Schlick(f0, fd90, NdotL).x;
	float viewScatter = F_Schlick(f0, fd90, NdotV).x;

	return lightScatter * viewScatter * energyFactor;
}


float3 pbrBrdf(float3 V, float3 L, float3 N, const __global Material* material)
{
	float3 f0 = material->pbr.reflectance;
	float f90 = 1.0f;
	float linearRoughness = 1.0f - material->pbr.smoothness;
	float roughness = linearRoughness * linearRoughness;


	// This code is an example of call of previous functions
	float NdotV = fabs(dot(N, V)) + 1e-5f; // avoid artifact
	float3 H = normalize(V + L);
	float LdotH = saturate(dot(L, H));
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));

	// Specular BRDF
	float3 F = F_Schlick(f0, f90, LdotH);
	float Vis = G_SmithGGXCorrelated (NdotV, NdotL, roughness);
	float D = D_GGX (NdotH , roughness);
	float3 Fr = D * F * Vis / PI;

	// Diffuse BRDF
	float Fd = Fr_DisneyDiffuse (NdotV, NdotL, LdotH, linearRoughness) / PI;

	return Fr + Fd * material->pbr.baseColour;
}

#endif // __PBR_BRDF_CL