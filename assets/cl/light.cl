#ifndef __LIGHT_CL
#define __LIGHT_CL
struct Light
{
	enum class Type
	{
		Point,
		Directional
	};

	Type type;
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
	}
}

bool getLightVector(const Light* light, const float3* position, const float3* direction)
{
	if (light.type == Light::Type::Point)
	{
		lightDirection = glm::normalize(light.point.position - position);
		return true;
	}
	else if (light.type == Light::Type::Directional)
	{
		lightDirection = -light.directional.direction;
		return true;
	}

	return false;
}

bool is_light_visible(const glm::vec3& position, const Light& light, const Scene& scene) {

	if (light.type == Light::Type::Directional) {
		Ray ray(position, -light.directional.direction);
		return !scene.check_ray(ray);
	}
	else if (light.type == Light::Type::Point) {
		Line line(position, light.point.position);
		return !scene.check_line(line);
	}
	return false;
}

/*float3 trace_light(const float3* position, const Light* light, const Scene* scene)
{
	if (light.type == Light::Type::Directional)
	{
		Ray ray(position, -light.directional.direction);
		return scene.trace_ray(ray);
	}
	else if (light.type == Light::Type::Point)
	{
		Line line(position, light.point.position);
		return scene.trace_ray(line);
	}
	return light.colour;
}*/
#endif// __LIGHT_CL