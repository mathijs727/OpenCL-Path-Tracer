// OpenCL float3 type is typedef'd to float4 so it is 16 bytes long unlike glm::vec3,
// which is 12 bytes. RawShapes are the structs as defined in the C++ program with glm.
// They should be converted to device native Shapes using convertRawShape(raw, device).
struct RawSphere
{
	float centre[3];
	float radius;
};
typedef struct RawSphere RawSphere;

void convertRawShape(const RawSphere* input, Sphere* output)
{
	output->radius = input->radius;
	output->centre.x = input->centre[0];
	output->centre.y = input->centre[1];
	output->centre.z = input->centre[2];
}