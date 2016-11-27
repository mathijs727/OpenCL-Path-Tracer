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
	MaterialType type;
	float3 colour;
	union
	{
		struct
		{
			float refractive_index;
		} fresnel;
	};
} Material;

typedef struct
{
	MaterialType type;
	float colour[3];
	union
	{
		struct
		{
			float refractive_index;
		} fresnel;
	};
} RawMaterial;

void convertRawMaterial(const RawMaterial* input, Material* output)
{
	output->type = input->type;
	output->colour.x = input->colour[0];
	output->colour.y = input->colour[1];
	output->colour.z = input->colour[2];
	if (input->type == Fresnel)
	{
		output->fresnel.refractive_index = input->fresnel.refractive_index;
	}
}
#endif// __MATERIAL_CL