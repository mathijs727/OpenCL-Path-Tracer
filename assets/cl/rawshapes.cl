#ifndef __RAWSHAPES_CL
#define __RAWSHAPES_CL
// OpenCL float3 type is typedef'd to float4 so it is 16 bytes long unlike glm::vec3,
// which is 12 bytes. RawShapes are the structs as defined in the C++ program with glm.
// They should be converted to device native Shapes using convertRawShape(raw, device).
typedef struct
{
	float centre[3];
	float radius;
} RawSphere;

typedef struct
{
	float normal[3];
	float offset;
} RawPlane;

void convertRawSphere(const RawSphere* input, Sphere* output)
{
	output->centre.x = input->centre[0];
	output->centre.y = input->centre[1];
	output->centre.z = input->centre[2];
	output->radius = input->radius;
}

void convertRawPlane(const RawPlane* input, Plane* output)
{
	output->normal.x = input->normal[0];
	output->normal.y = input->normal[1];
	output->normal.z = input->normal[2];
	output->offset = input->offset;
}
#endif// __RAWSHAPES_CL