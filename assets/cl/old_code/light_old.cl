#ifndef __LIGHT_OLD_CL
#define __LIGHT_OLD_CL

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
			float sqrAttRadius;
			float invSqrAttRadius;
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
void getShadowRay(const Light* light, float3 intersection, Ray* outRay, float* outMaxT)
{
	if (light->type == PointLightType)
	{
		float3 direction = light->point.position - intersection;
		outRay->origin = intersection;
		outRay->direction = normalize(direction);
		*outMaxT = sqrt(dot(direction, direction));
	} else {
		outRay->origin = intersection;
		outRay->direction = -light->directional.direction;
		*outMaxT = INFINITY;
	}
}

bool isLightCulled(const Light* light, float3 intersection)
{
	if (light->type == PointLightType)
	{
		float3 dir = (light->point.position - intersection);
		float distSquare = dot(dir, dir);
		return (distSquare > light->point.sqrAttRadius);
	} else {
		return false;
	}
}

// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
float smoothDistanceAtt(float squaredDistance, float invSqrAttRadius)
{
	float factor = squaredDistance * invSqrAttRadius ;
	float smoothFactor = clamp(1.0f - factor * factor, 0.0f, 1.0f);
	return smoothFactor * smoothFactor ;
}

// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
float getDistanceAtt(float3 unormalizedLightVector, float invSqrAttRadius)
{
	float sqrDist = dot(unormalizedLightVector , unormalizedLightVector);
	float attenuation = 1.0f / (max(sqrDist, 0.01f*0.01f));
	attenuation *= smoothDistanceAtt(sqrDist, invSqrAttRadius);
	return attenuation;
}

float3 getLightIntensity(const Light* light, float3 intersection)
{
	if (light->type == PointLightType)
	{
		float3 dir = (light->point.position - intersection);
		return getDistanceAtt(dir, light->point.invSqrAttRadius);
	} else {
		return light->colour;
	}
}
#endif// __LIGHT_OLD_CL