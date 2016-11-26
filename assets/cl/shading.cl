#ifndef __SHADING_CL
#define __SHADING_CL
#include "light.cl"
#include "material.cl"

float3 diffuseShade(
	const float3* rayDirection,
	const float3* intersection,
	const float3* normal,
	int numLights,
	__local const Light* lights,
	const float3* diffuseColour)
{
	float3 result = (float3)(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < numLights; i++)
	{
		Light light = lights[i];
		float3 lightDir = getLightVector(&light, intersection);
		float NdotL = max(0.0f, dot(*normal, lightDir));
		result += NdotL * light.colour;
	}
	return *diffuseColour * result;
}

float3 whittedShading(
	const float3* rayDirection,
	const float3* intersection,
	const float3* normal,
	int numLights,
	__local const Light* lights,
	const Material* material)
{
	if (material->type == Diffuse)
	{
		const float3* matColour = &material->colour;
		return diffuseShade(rayDirection, intersection, normal, numLights, lights, matColour);
	} else {
		return (float3)(1.0f, 0.0f, 0.0f);
	}
}
#endif