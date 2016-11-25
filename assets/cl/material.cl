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
} Material;

typedef struct
{
	MaterialType type;
	float colour[3]; 
} RawMaterial;

void convertRawMaterial(const RawMaterial* input, Material* output)
{
	output->type = input->type;
	output->colour.x = input->colour[0];
	output->colour.y = input->colour[1];
	output->colour.z = input->colour[2];
}