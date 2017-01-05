#ifndef __MATERIAL_CL
#define __MATERIAL_CL
typedef enum {
		Diffuse,
		Emisive
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
			float3 emisiveColour;
		} emisive;
	};
	MaterialType type;
} Material;
#endif// __MATERIAL_CL