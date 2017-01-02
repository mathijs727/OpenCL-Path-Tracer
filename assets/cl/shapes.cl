#ifndef __SHAPES_CL
#define __SHAPES_CL
#include "ray.cl"

#define PI 3.14159265359f

typedef struct
{
	uint indices[3];
	uint mat_index;
} TriangleData;

typedef struct
{
	float3 vertex;
	float3 normal;
	float2 texCoord;
} VertexData;

bool intersectRayTriangle(
	const Ray* ray,
	const VertexData* vertices,
	float* outT,
	float2* outUV) {
	float3 O = ray->origin;
	float3 D = ray->direction;
	float3 V1 = vertices[0].vertex;
	float3 V2 = vertices[1].vertex;
	float3 V3 = vertices[2].vertex;
	float3 e1, e2;  //Edge1, Edge2
	float3 P, Q, T;
	float det, inv_det, u, v;
	float t;

	//Find vectors for two edges sharing V1
	e1 = V2 - V1;
	e2 = V3 - V1;
	//Begin calculating determinant - also used to calculate u parameter
	P = cross(D,e2);
	//if determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
	det = dot(e1, P);
	//NOT CULLING
	if(det > -FLT_MIN && det < FLT_MIN) return false;
	inv_det = 1.f / det;

	//calculate distance from V1 to ray origin
	T = O - V1;

	//Calculate u parameter and test bound
	u = dot(T, P) * inv_det;
	//The intersection lies outside of the triangle
	if(u < 0.f || u > 1.f) return false;

	//Prepare to test v parameter
	Q = cross(T, e1);

	//Calculate V parameter and test bound
	v = dot(D, Q) * inv_det;
	//The intersection lies outside of the triangle
	if(v < 0.f || u + v  > 1.f) return false;

	t = dot(e2, Q) * inv_det;

	if(t > 0.f) { //ray intersection
		*outT = t;
		*outUV = (float2)(u, v);
		return true;
	}

	// No hit, no win
	return false;
}

#endif// __SHAPES_CL