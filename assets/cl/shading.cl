#ifndef __SHADING_CL
#define __SHADING_CL
#include "light.cl"
#include "material.cl"
#include "stack.cl"
#include "scene.cl"

#define RAYTRACER_EPSILON 0.0001f

__constant sampler_t sampler =
	CLK_NORMALIZED_COORDS_TRUE |
	CLK_ADDRESS_REPEAT |
	CLK_FILTER_LINEAR;

float3 diffuseShade(
	float3 rayDirection,
	float3 intersection,
	float3 normal,
	const Scene* scene,
	float3 diffuseColour)
{
	float3 result = (float3)(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < scene->numLights; i++)
	{
		Light light = scene->lights[i];
		if (isLightCulled(&light, intersection))
			continue;
		
		bool lightVisible = false;
		Ray ray;
		float maxT;
		getShadowRay(&light, intersection, &ray, &maxT);

#ifdef NO_SHADOWS
		lightVisible = true;
#else
	#ifdef COUNT_TRAVERSAL
		lightVisible = !traceRay(scene, &ray, true, maxT, NULL, NULL, NULL, NULL, NULL);
	#else
		lightVisible = !traceRay(scene, &ray, true, maxT, NULL, NULL, NULL, NULL);
	#endif
#endif

		if (lightVisible)
		{
			float3 lightDir = getLightVector(&light, intersection);
			float NdotL = max(0.0f, dot(normal, lightDir));
			result += NdotL * getLightIntensity(&light, intersection);
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
	const Scene* scene,
	int triangleIndex,
	float3 intersection,
	const __global float* invTransform,
	float3 rayDirection,
	float2 uv,
	image2d_array_t textures,
	float3 multiplier,
	Stack* stack)
{
	// Retrieve the material and calculate the texture coords and normal
	const __global Material* material = &scene->meshMaterials[scene->triangles[triangleIndex].mat_index];
	VertexData vertices[3];
	TriangleData triangle = scene->triangles[triangleIndex];
	getVertices(vertices, triangle.indices, scene);
	float2 t0 = vertices[0].texCoord;
	float2 t1 = vertices[1].texCoord;
	float2 t2 = vertices[2].texCoord;
	float2 tex_coords = t0 + (t1-t0) * uv.x + (t2-t0) * uv.y;
	float3 n0 = vertices[0].normal;
	float3 n1 = vertices[1].normal;
	float3 n2 = vertices[2].normal;
	float3 normal = normalize( n0 + (n1-n0) * uv.x + (n2-n0) * uv.y );

	float normalTransform[16];
	matrixTranspose(invTransform, normalTransform);
	normal = normalize(matrixMultiplyLocal(normalTransform, (float4)(normal, 0.0f)).xyz);

	if (material->type == Diffuse) {
		float3 matColour;
		if (material->diffuse.tex_id == -1)
		{
			matColour = material->colour;
		} else {
			float4 texCoords3d = (float4)(1.0f - tex_coords.x, tex_coords.y, material->diffuse.tex_id, 0.0f);
			float4 colourWithAlpha = read_imagef(
				textures,
				sampler,
				texCoords3d);
			matColour = colourWithAlpha.xyz;
		}
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
			/*if (type == SphereType)
			{
				Sphere sphere = scene->spheres[shape_index];
				intersect_inside_distance = intersectInsideSphere(&refractive_ray, &sphere, &intersect_inside_normal);
			} else if (type == PlaneType) {
				Plane plane = scene->planes[shape_index];
				intersect_inside_distance = intersectInsidePlane(&refractive_ray, &plane, &intersect_inside_normal);
			} else {
				return (float3)(0.0f, 0.0f, 0.0f);
			}*/
			// TODO: implement fresnel for triangle meshes
			return (float3)(0.0f, 0.0f, 0.0f);
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
				StackItem item1;
				calcReflectiveRay(rayDirection, intersection, normal, &item1.ray);
				item1.multiplier = multiplier * material->colour * reflection_coeff;
				StackPush(stack, &item1);

				//Beer's law
				float3 ab = material->fresnel.absorption;

				float3 beer_factor;
				beer_factor.x = exp( -ab.x * intersect_inside_distance );
				beer_factor.y = exp( -ab.y * intersect_inside_distance );
				beer_factor.z = exp( -ab.z * intersect_inside_distance );

				StackItem item2;
				item2.ray = second_refractive_ray;
				item2.multiplier = multiplier * material->colour * (1.0f - reflection_coeff) * beer_factor;
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