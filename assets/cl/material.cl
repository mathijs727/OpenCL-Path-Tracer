#ifndef __MATERIAL_CL
#define __MATERIAL_CL
typedef enum {
		DIFFUSE,
		PBR,
		REFRACTIVE,
		BASIC_REFRACTIVE,
		EMISSIVE
} MaterialType;

typedef struct
{
	union
	{
		struct
		{
			float3 diffuseColour;
			int tex_id;
		} diffuse;
		struct
		{
			// Metallic is a boolean flag
			// Applied as "Hodgman" suggests (not well documented in any PBR papers from Unreal or Dice):
			// https://www.gamedev.net/topic/672836-pbr-metalness-equation/
			union
			{
				float3 baseColour;// Non metal
				float3 reflectance;// Metal (f0 for Schlick approximation of Fresnel)
			};
			float smoothness;
			float f0NonMetal;
			bool metallic;
		} pbr;
		struct
		{
			float3 absorption;
			float smoothness;
			float refractiveIndex;
		} refractive;
		struct
		{
			float3 absorption;
			float refractiveIndex;
		} basicRefractive;
		struct
		{
			float3 emissiveColour;
		} emissive;
	};
	MaterialType type;
} Material;
#endif// __MATERIAL_CL