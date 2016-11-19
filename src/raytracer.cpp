#include "raytracer.h"

#include "camera.h"
#include "scene.h"
#include "template/surface.h"
#include "ray.h"
#include "pixel.h"
#include <algorithm>
#include <emmintrin.h>

using namespace raytracer;

void raytracer::raytrace(const Camera& camera, const Scene& scene, Tmpl8::Surface& target_surface) {
	glm::vec3 eye;
	glm::vec3 scr_base_origin;
	glm::vec3 scr_base_u;
	glm::vec3 scr_base_v;
	camera.get_frustum(eye, scr_base_origin, scr_base_u, scr_base_v);

	glm::vec3 u_step = scr_base_u / (float)target_surface.GetWidth();
	glm::vec3 v_step = scr_base_v / (float)target_surface.GetHeight();

	float f_255 = 255.0f;
	i32 i_255[4] = { 255, 255, 255, 255 };
	__m128 sse_f_255 = _mm_load1_ps(&f_255);
	__m128i sse_i_255 = _mm_loadu_si128((__m128i*)i_255);

	u32* buffer = target_surface.GetBuffer();
	for (int y = 0; y < target_surface.GetHeight(); ++y) {
		for (int x = 0; x < target_surface.GetWidth(); ++x) {
			glm::vec3 scr_point = scr_base_origin + u_step * (float) x + v_step * (float) y;
			Ray ray(eye, glm::normalize(scr_point - eye));
			glm::vec3 colour = scene.trace_ray(ray);

			PixelRGBA pixel;
#if TRUE
			pixel.r = (u8) std::min((u32)(colour.r * 255), 255U);
			pixel.g = (u8) std::min((u32)(colour.g * 255), 255U);
			pixel.b = (u8) std::min((u32)(colour.b * 255), 255U);
			pixel.a = 255;
			*buffer = pixel.value;
#else
			float f_color[4];
			f_color[0] = colour.r;
			f_color[1] = colour.g;
			f_color[2] = colour.b;
			__m128 sse_f_colour = _mm_loadu_ps(f_color);
			sse_f_colour = _mm_mul_ps(sse_f_colour, sse_f_255);// Mul with 255
			__m128i sse_i_colour = _mm_cvtps_epi32(sse_f_colour);// Cast to int
			sse_i_colour = _mm_min_epi32(sse_i_colour, sse_i_255);// Min
			sse_i_colour = _mm_packus_epi32(sse_i_colour, sse_i_colour);// i32 to u16
			sse_i_colour = _mm_packus_epi16(sse_i_colour, sse_i_colour);// u16 to u8
			u8 u8_color[16];
			_mm_storeu_si128((__m128i*)u8_color, sse_i_colour);

			memcpy(buffer, u8_color, sizeof(PixelRGBA));
#endif
			++buffer;
		}
	}
}
