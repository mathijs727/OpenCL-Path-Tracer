#ifndef __SHADING_CL
#define __SHADING_CL
#include "light.cl"
#include "material.cl"
#include "stack.cl"

#define RAYTRACER_EPSILON 0.00001f

float3 diffuseShade(
	float3 rayDirection,
	float3 intersection,
	float3 normal,
	__local const Scene* scene,
	float3 diffuseColour)
{
	return (float3)(1.0f, 1.0f, 1.0f);
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
			line.origin += normal * RAYTRACER_EPSILON;
			lightVisible = !checkLine(scene, &line);
		} else {
			ray.origin += normal * RAYTRACER_EPSILON;
			lightVisible = !checkRay(scene, &ray);
		}

		if (lightVisible)
		{
			float3 lightDir = getLightVector(&light, intersection);
			float NdotL = max(0.0f, dot(normal, lightDir));
			result += NdotL * light.colour;
		}
	}
	return diffuseColour * result;
}

// http://math.stackexchange.com/questions/13261/how-to-get-a-reflection-vector
float3 reflect(const float3 I, const float3 N)
{
	return I - 2 * dot(I, N) * N;
}

void calcReflectiveRay(
	float3 rayDirection,
	float3 intersection,
	float3 normal,
	Ray* ray)
{
	ray->origin = intersection + normal * RAYTRACER_EPSILON;
	ray->direction = reflect(rayDirection, normal);
}

bool calcRefractiveRay(
	float n1,
	float n2,
	float3 rayDirection,
	float3 intersection,
	float3 normal,
	Ray* outRay)
{
	float cosine = dot(-rayDirection, normal);
	float refractive_ratio = n1 / n2;
	float k = 1 - ( pow(refractive_ratio,2) * (1 - pow(cosine,2)) ); 
	if (k >= 0) {
		outRay->direction = normalize(rayDirection * refractive_ratio + normal * (refractive_ratio * cosine - sqrt(k)));
		outRay->origin = intersection - normal * RAYTRACER_EPSILON;
		return true;
	}
	else return false;
}

float3 whittedShading(
	__local const Scene* scene,
	float3 rayDirection,
	float3 intersection,
	float3 normal,
	ShapeType type,
	const void* shape,
	const Material* material,
	float3 multiplier,
	Stack* stack)
{
	if (material->type == Diffuse) {
		float3 matColour = material->colour;
		
		return multiplier * diffuseShade(rayDirection, intersection, normal, scene, matColour);
	} else if (material->type == Reflective) {
		float3 matColour = material->colour;

		StackItem item;
		calcReflectiveRay(rayDirection, intersection, normal, &item.ray);
		item.multiplier = multiplier * matColour;
		StackPush(stack, &item);
		return (float3)(0.0f, 0.0f, 0.0f);
	} else if (material->type == Glossy) {
		StackItem item;
		calcReflectiveRay(rayDirection, intersection, normal, &item.ray);
		item.multiplier = multiplier * material->colour * material->glossy.specularity;
		StackPush(stack, &item);

		float3 diffuseComponent = (1 - material->glossy.specularity) * \
			diffuseShade(rayDirection, intersection, normal, scene, material->colour);
		return multiplier * diffuseComponent;
	} else if (material->type == Fresnel) {
		float n1 = scene->refractiveIndex;
		float n2 = material->fresnel.refractiveIndex;
		float cosine = dot(normal, -rayDirection);
		Ray refractive_ray;
		if (calcRefractiveRay(n1, n2, rayDirection, intersection, normal, &refractive_ray)) {
			float r0 = pow((n1 - n2) / (n2 + n1), 2);
			float reflection_coeff = r0 + (1 - r0)*pow(1 - cosine, 5);
			
			float3 intersect_inside_normal;
			float intersect_inside_distance;
			if (type == SphereType)
			{
				intersectInsideSphere(&refractive_ray, (Sphere*)shape, &intersect_inside_normal);
			} else if (type == PlaneType) {
				intersectInsidePlane(&refractive_ray, (Plane*)shape, &intersect_inside_normal);
			} else {
				return (float3)(0.0f, 0.0f, 0.0f);
			}
			float3 inside_refraction_point = refractive_ray.origin + refractive_ray.direction * intersect_inside_distance;

			Ray second_refractive_ray;
			bool good_ray = calcRefractiveRay(
				n1,
				n2,
				refractive_ray.direction,
				inside_refraction_point,
				intersect_inside_normal,
				&second_refractive_ray);
			if (good_ray) {
				//glm::vec3 reflective_component = reflection_coeff * scene.trace_ray(calc_reflective_ray(rayDirection, intersection, normal), current_depth);
				//glm::vec3 refractive_component = (1 - reflection_coeff) * scene.trace_ray(second_refractive_ray, current_depth);
				//return material.colour * (refractive_component + reflective_component);
				Ray ray;
				StackItem item1;
				calcReflectiveRay(rayDirection, intersection, normal, &item1.ray);
				item1.multiplier = multiplier * material->colour * reflection_coeff;
				StackPush(stack, &item1);

				StackItem item2;
				item2.ray = second_refractive_ray;
				item2.multiplier = multiplier * material->colour * (1.0f - reflection_coeff);
				StackPush(stack, &item2);
				return (float3)(0.0f, 0.0f, 0.0f);
			}
		}
		else {
			StackItem item;
			calcReflectiveRay(rayDirection, intersection, normal, &item.ray);
			item.multiplier = multiplier * material->colour;
			StackPush(stack, &item);
			//material->colour * scene.trace_ray(&ray, current_depth);

			return (float3)(0.0f, 0.0f, 0.0f);
		}

		/*float n1 = scene->refractive_index;
		float n2 = material->fresnel.refractive_index;
		float cosine = dot(normal, -rayDirection);
		Ray refractive_ray;
		if (calc_refractive_ray(n1, n2, rayDirection, intersection, normal, refractive_ray)) {
			float r0 = pow((n1 - n2) / (n2 + n1), 2);
			float reflection_coeff = r0 + (1 - r0)*pow(1 - cosine, 5);
			assert(0 < reflection_coeff && reflection_coeff < 1);
			
			glm::vec3 intersect_inside_normal;
			float intersect_inside_distance = intersect_inside(refractive_ray, shape, intersect_inside_normal);
			glm::vec3 inside_refraction_point = refractive_ray.origin + refractive_ray.direction * intersect_inside_distance;

			Ray second_refractive_ray;
			bool good_ray = calc_refractive_ray(n1, n2, refractive_ray.direction, inside_refraction_point, intersect_inside_normal, second_refractive_ray);
			assert(good_ray);
			if (good_ray) {
				glm::vec3 reflective_component = reflection_coeff * scene.trace_ray(calc_reflective_ray(rayDirection, intersection, normal), current_depth);
				glm::vec3 refractive_component = (1 - reflection_coeff) * scene.trace_ray(second_refractive_ray, current_depth);
				return material.colour * (refractive_component + reflective_component);
			}
			else return glm::vec3(0,0,0); // should not happen
		}
		else {
			material.colour * scene.trace_ray(calc_reflective_ray(rayDirection, intersection, normal), current_depth);
		}*/
	}
	return (float3)(1.0f, 0.0f, 0.0f);	
}
#endif