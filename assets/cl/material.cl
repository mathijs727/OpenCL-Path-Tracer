#ifndef __MATERIAL_CL
#define __MATERIAL_CL
typedef enum {
		Diffuse,
		Emmisive
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
			float3 emmisiveColour;
		} emmisive;
	};
	MaterialType type;
} Material;
#endif// __MATERIAL_CL