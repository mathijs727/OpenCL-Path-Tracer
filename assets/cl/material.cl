#ifndef __MATERIAL_CL
#define __MATERIAL_CL
typedef enum {
		Diffuse,
		Emissive
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
			float3 emissiveColour;
		} emissive;
	};
	MaterialType type;
} Material;
#endif// __MATERIAL_CL