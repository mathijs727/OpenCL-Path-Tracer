#ifndef __MATERIAL_CL
#define __MATERIAL_CL
typedef enum {
		Reflective,
		Diffuse,
		Glossy,
		Fresnel
} MaterialType;

typedef struct
{
	float3 colour;
	union
	{
		struct
		{
			int tex_id;
		} diffuse;
		struct
		{
			float specularity;
		} glossy;
		struct
		{
			float3 absorption;
			float refractiveIndex;
		} fresnel;
	};
	MaterialType type;
} Material;
#endif// __MATERIAL_CL