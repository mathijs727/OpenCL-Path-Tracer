#ifndef __SHADING_CL
#define __SHADING_CL
#include "light.cl"
#include "material.cl"

float3 diffuseShade(
	const float3* rayDirection,
	const float3* intersection,
	const float3* normal,
	__local const Scene* scene,
	const float3* diffuseColour)
{
	float3 result = (float3)(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < scene->numLights; i++)
	{
		Light light = scene->lights[i];
		
		// Shadow test
		bool lightVisible = false;
		Line line;
		Ray ray;
		int rayOrLine = getShadowLineRay(&light, intersection, &line, &ray);
		if (rayOrLine == 1)
		{
			line.origin += *normal * 0.00001f;
			lightVisible = !checkLine(scene, &line);
		} else {
			ray.origin += *normal * 0.00001f;
			lightVisible = !checkRay(scene, &ray);
		}

		if (lightVisible)
		{
			float3 lightDir = getLightVector(&light, intersection);
			float NdotL = max(0.0f, dot(*normal, lightDir));
			result += NdotL * light.colour;
		}
	}
	return *diffuseColour * result;
}

float3 whittedShading(
	const float3* rayDirection,
	const float3* intersection,
	const float3* normal,
	__local const Scene* scene,
	const Material* material)
{
	if (material->type == Diffuse)
	{
		const float3* matColour = &material->colour;
		return diffuseShade(rayDirection, intersection, normal, scene, matColour);
	} else {
		return (float3)(1.0f, 0.0f, 0.0f);
	}
}
#endif