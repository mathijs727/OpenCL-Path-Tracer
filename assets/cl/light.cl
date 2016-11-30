#ifndef __LIGHT_CL
#define __LIGHT_CL

typedef enum
{
	PointLightType,
	DirectionalLightType
} LightType;

typedef struct
{
	float3 colour;
	union
	{
		struct
		{
			float3 position;
		} point;
		struct
		{
			float3 direction;
		} directional;
	};
	LightType type;
} Light;

float3 getLightVector(const Light* light, float3 intersection)
{
	if (light->type == PointLightType)
	{
		return normalize(light->point.position - intersection);
	} else {
		return -light->directional.direction;
	}
}

// Return 1 for line, 2 for ray, 0 for error
int getShadowLineRay(const Light* light, float3 intersection, Line* line, Ray* ray)
{
	if (light->type == PointLightType)
	{
		line->origin = intersection;
		line->dest = light->point.position;
		return 1;
	} else {
		ray->origin = intersection;
		ray->direction = -light->directional.direction;
		return 2;
	}
	return 0;
}

float getLightFallOff(const Light* light, float3 intersection)
{
	if (light->type == PointLightType)
	{
		float3 dir = (light->point.position - intersection);
		float distSquare = dot(dir, dir);
		return 1.0f / distSquare;
	} else {
		return 1.0f;
	}
}
#endif// __LIGHT_CL