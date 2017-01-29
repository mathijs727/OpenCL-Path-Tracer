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

// http://igorsklyar.com/system/documents/papers/28/Schlick94.pdf
float G_Schlick(float VdotN, float alpha) {
	float k = sqrt(2*alpha*alpha/PI);
	return VdotN / (VdotN*(1-k)+k);
}

float G_SmithBeckmannCorrelated(float VdotM, float NdotV, float alpha) {
	float a = 1.0f/(alpha*tan(acos(NdotV)));
	float chi = a > 0 ? 1.0f : 0.0f;
	float approximation = 1.0f;
	if (a < 1.6f) {
		approximation = (3.535f * a + 2.181f * a * a) / ( 1 + 2.276f * a + 2.577f * a * a);
	}
	//if (NdotV > EPSILON)
		return chi * VdotM / NdotV * approximation;
	//else return 1.0f;
}

float G_SmithGGXCorrelated(float NdotL, float NdotV, float alpha)
{
	// Height correlated Smith GGX geometry term as defined in (equation 3 of section 3.1.2):
	// Course notes "Moving Frostbite to PBR" by DICE
	// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
	float alpha2 = alpha * alpha;
	float NdotL2 = NdotL * NdotL;
	float NdotV2 = NdotV * NdotV;

	// Original formulation of G_SmithGGX Correlated
	float lambda_v = (-1 + sqrt(1 + alpha2 * (1 - NdotV2) / NdotV2)) / 2.0f;
	float lambda_l = (-1 + sqrt(1 + alpha2 * (1 - NdotL2) / NdotL2)) / 2.0f;
	float G_SmithGGXCorrelated = 1 / (1 + lambda_v + lambda_l);
	//float V_SmithGGXCorrelated = G_SmithGGXCorrelated;

	// Heavyside function f(a) = 1 if a > 0 else 0
	// Last row of table 1 of "Movign Frostbite to PBR"
	// Does not have to be applied here since we do it in the main function?
	//if (NdotL <= 0 || NdotV <= 0) {
	//	return 0.0f;
	//} else {
		return G_SmithGGXCorrelated;
	//}

	
	/*// This is the optimize version (including divide by 4 * NdotL * NdotV)
	// Caution : the " NdotL *" and " NdotV *" are explicitely inversed , this is not a mistake .
	float Lambda_GGXV = NdotL * sqrt((-NdotV * alpha2 + NdotV) * NdotV + alpha2);
	float Lambda_GGXL = NdotV * sqrt((-NdotL * alpha2 + NdotL) * NdotL + alpha2);

	return 0.5f / (Lambda_GGXV + Lambda_GGXL);*/
}

float D_Beckmann(float NdotH, float alpha) 
{
	float tanA = tan(acos(NdotH));
	float cos2 = NdotH*NdotH;
	float num = (1 - cos2) / (cos2*alpha*alpha);
	float den = cos2*cos2*alpha*alpha*PI;
	if (den > EPSILON)
		return exp(-num)/den;
	else return 1.0f;

}

float D_GGX (float NdotH, float alpha)
{
	// GGX (Trowbridge-Reitz) As described here:
	// http://graphicrants.blogspot.nl/2013/08/specular-brdf-reference.html
	float alpha2 = alpha * alpha;
	float f = (NdotH*NdotH) * (alpha2 - 1) + 1;
	if (f > EPSILON)
		return alpha2 / (PI * f * f);
	else return 1.0f;
}

float Fr_DisneyDiffuse(float NdotV, float NdotL, float LdotH, float linearRoughness)
{
	float energyBias = lerp(0, 0.5f , linearRoughness );
	float energyFactor = lerp(1.0f , 1.0f / 1.51f , linearRoughness );
	float fd90 = energyBias + 2.0f * LdotH * LdotH * linearRoughness;
	float3 f0 = (float3)(1.0f, 1.0f, 1.0f);
	float lightScatter = F_Schlick(f0, fd90, NdotL).x;
	float viewScatter = F_Schlick(f0, fd90, NdotV).x;

	return lightScatter * viewScatter * energyFactor;
}


// Moving frostbite to PBR:
// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
//
// Explains how to combine Specular + Diffuse (and why) (Siggraph Shading Course 2013):
// http://blog.selfshadow.com/publications/s2013-shading-course/hoffman/s2013_pbs_physics_math_slides.pdf
//
// Tells us f0=0.04f is a good approximation for dielectrics (set in cpp code):
// https://de45xmedrsdbp.cloudfront.net/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
//
// Tells us linear roughness is sqrt of roughness (and not square):
// https://seblagarde.wordpress.com/2014/04/14/dontnod-physically-based-rendering-chart-for-unreal-engine-4/
float3 pbrBrdf(
	float3 V,
	float3 L,
	float3 N,
	const __global Material* material)
{
	float3 f0;
	if (material->pbr.metallic) {
		f0 = material->pbr.reflectance;
	} else {
		f0 = material->pbr.f0NonMetal;
	}
	float f90 = 1.0f;
	float roughness = 1.0f - material->pbr.smoothness;
	float linearRoughness = sqrt(roughness);

	float NdotV = fabs(dot(N, V)) + 1e-5f; // avoid artifact
	float3 H = normalize(V + L);
	float LdotH = saturate(dot(L, H));
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));
	float VdotH = saturate(dot(V, H));

	// Specular BRDF
	float3 F = F_Schlick(f0, f90, LdotH);
	//float G = G_SmithGGXCorrelated(NdotL, NdotV, roughness);
	float G = G_SmithBeckmannCorrelated(VdotH, NdotV, roughness)*G_SmithBeckmannCorrelated(LdotH, NdotL, roughness);
	//float D = D_GGX (NdotH , roughness);
	float D = D_Beckmann (NdotH, roughness);
	// The G function might return 0.0f because of the heavyside function.
	// Since OpenCL math is not that strict (and we dont want it to be for performance reasons),
	//  multiplying by 0.0f (which G may return) does not mean that Fr will actually become 0.0f.
	float3 Fr;
	if (NdotL * NdotV > EPSILON)
	{
		Fr = D * F * G / (4.0f * NdotL * NdotV);
	} else {
		Fr = 0.0f;
	}

	// Diffuse BRDF (called BRDF because it assumes light enters and exists at the same point,
	//  but it tries to approximate diffuse scatering so maybe this should be called BTDF and
	//  call the whole function BSDF (which is BRDF + BTDF))
	float Fd = Fr_DisneyDiffuse(NdotV, NdotL, LdotH, linearRoughness) / PI;
	float3 diffuseColour;
	if (material->pbr.metallic)
	{
		diffuseColour = (float3)(0.0f, 0.0f, 0.0f);
	} else {
		diffuseColour = material->pbr.baseColour;
	}
	float3 reflected = Fr;
	float3 diffuse = (1.0f - F) * (Fd * diffuseColour);

	return reflected + diffuse;
}


float3 brdfOnly(
	float3 V,
	float3 H,
	float3 L,
	float3 N,
	const __global Material* material)
{
	float3 f0;
	if (material->pbr.metallic) {
		f0 = material->pbr.reflectance;
	} else {
		f0 = material->pbr.f0NonMetal;
	}
	float f90 = 1.0f;
	float roughness = 1.0f - material->pbr.smoothness;


	float NdotV = fabs(dot(N, V)) + 1e-5f; // avoid artifact
	float LdotH = saturate(dot(L, H));
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));
	float VdotH = saturate(dot(V, H));
	// Specular BRDF
	//float3 F = F_Schlick(f0, f90, LdotH);
	//float G = G_SmithGGXCorrelated(NdotL, NdotV, roughness);
	float G = G_SmithBeckmannCorrelated(VdotH, NdotV, roughness)*G_SmithBeckmannCorrelated(LdotH, NdotL, roughness);
	//float D = D_GGX (NdotH , roughness);
	//float D = D_Beckmann (NdotH, roughness);
	float D = 1.0f;
	// The G function might return 0.0f because of the heavyside function.
	// Since OpenCL math is not that strict (and we dont want it to be for performance reasons),
	//  multiplying by 0.0f (which G may return) does not mean that Fr will actually become 0.0f.
	float3 Fr;
	float denom = NdotL * NdotV;
	if (denom > EPSILON)
	{
		denom = fmax(0.05f, denom);
		return D * G / (4.0f * denom);
	}
	else {
		return BLACK;
	}
}

float3 diffuseOnly(
	float3 V,
	float3 L,
	float3 N,
	const __global Material* material)
{
	float3 f0;
	if (material->pbr.metallic) {
		f0 = material->pbr.reflectance;
	} else {
		f0 = material->pbr.f0NonMetal;
	}
	float f90 = 1.0f;
	float roughness = 1.0f - material->pbr.smoothness;
	float linearRoughness = sqrt(roughness);

	float NdotV = fabs(dot(N, V)) + 1e-5f; // avoid artifact
	float3 H = normalize(V + L);
	float LdotH = saturate(dot(L, H));
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));

	//float3 F = F_Schlick(f0, f90, LdotH);

	// Diffuse BRDF (called BRDF because it assumes light enters and exists at the same point,
	//  but it tries to approximate diffuse scatering so maybe this should be called BTDF and
	//  call the whole function BSDF (which is BRDF + BTDF))
	if (material->pbr.metallic)
	{
		return BLACK;
	}
	else {
		float Fd = Fr_DisneyDiffuse(NdotV, NdotL, LdotH, linearRoughness) / PI;
		return Fd * material->pbr.baseColour;
	}
}


#endif // __PBR_BRDF_CL