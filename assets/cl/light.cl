#ifndef __LIGHT_CL
#define __LIGHT_CL

typedef enum
{
	PointLightType,
	DirectionalLightType
} LightType;

typedef struct
{
	LightType type;
	float colour[3];
	union
	{
		struct
		{
			float position[3];
		} point;
		struct
		{
			float direction[3];
		} directional;
	};
} RawLight;

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

/*typedef struct
{
	float3 colour;
	float3 position;
} PointLight;

typedef struct
{
	float3 colour;
	float3 direction;
} DirectionalLight;

void convertRawPointLight(const RawLight* input, PointLight* output)
{
	if (input->type != PointLightType)
		return;

	output->colour.x = input->colour[0];
	output->colour.y = input->colour[1];
	output->colour.z = input->colour[2];
	output->position.x = input->point.position[0];
	output->position.y = input->point.position[1];
	output->position.z = input->point.position[2];
}

void convertRawDirectionalLight(const RawLight* input, DirectionalLight* output)
{
	if (input->type != DirectionalLightType)
		return;

	output->colour.x = input->colour[0];
	output->colour.y = input->colour[1];
	output->colour.z = input->colour[2];
	output->direction.x = input->directional.direction[0];
	output->direction.y = input->directional.direction[1];
	output->direction.z = input->directional.direction[2];
}*/
void convertRawLight(const RawLight* input, Light* output)
{
	output->type = input->type;
	output->colour.x = input->colour[0];
	output->colour.y = input->colour[1];
	output->colour.z = input->colour[2];

	if (input->type == PointLightType)
	{
		output->point.position.x = input->point.position[0];
		output->point.position.y = input->point.position[1];
		output->point.position.z = input->point.position[2];
	} else {
		output->directional.direction.x = input->directional.direction[0];
		output->directional.direction.y = input->directional.direction[1];
		output->directional.direction.z = input->directional.direction[2];
	}
}

/*float3 getLightVectorPoint(const PointLight* light, const float3* position)
{
	return normalize(light->position - *position);
}

float3 getLightVectorDirectional(const DirectionalLight* light, const float3* position)
{
	return -light->direction;
}*/

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

/*void getShadowRay(const float3 intersection, const DirectionalLight* light, Ray* ray)
{
	ray->origin = intersection;
	ray->direction = -light->direction;
}

void getShadowLine(const float3 intersection, const PointLight* light, Line* line)
{
	line->origin = intersection;
	line->dest = light->position;
}*/
#endif// __LIGHT_CL