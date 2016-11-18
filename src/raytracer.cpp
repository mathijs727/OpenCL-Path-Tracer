#include "raytracer.h"

#include "camera.h"
#include "scene.h"
#include "template/surface.h"
#include "ray.h"
#include "pixel.h"
#include <algorithm>

using namespace raytracer;

void raytracer::raytrace(const Camera& camera, const Scene& scene, Tmpl8::Surface& target_surface) {
	glm::vec3 eye;
	glm::vec3 scr_base_origin;
	glm::vec3 scr_base_u;
	glm::vec3 scr_base_v;
	camera.get_frustum(eye, scr_base_origin, scr_base_u, scr_base_v);
	
	glm::vec3 u_step = scr_base_u / (float) target_surface.GetWidth();
	glm::vec3 v_step = scr_base_v / (float) target_surface.GetHeight();

	u32* buffer = target_surface.GetBuffer();
	for (int y = 0; y < target_surface.GetHeight(); ++y) {
		for (int x = 0; x < target_surface.GetWidth(); ++x) {
			glm::vec3 scr_point = scr_base_origin + u_step * (float) x + v_step * (float) y;
			Ray ray(eye, glm::normalize(scr_point - eye));
			glm::vec3 colour = scene.trace_ray(ray);

			PixelRGBA pixel;
			pixel.r = (u8) std::min((u32)(colour.r * 255), 255U);
			pixel.g = (u8) std::min((u32)(colour.g * 255), 255U);
			pixel.b = (u8) std::min((u32)(colour.b * 255), 255U);
			pixel.a = 255;
			*buffer = pixel.value;
			++buffer;
		}
	}
}
