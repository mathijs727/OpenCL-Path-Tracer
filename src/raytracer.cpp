#include "raytracer.h"

#include "camera.h"
#include "scene.h"
#include "template/template.h"
#include "template/surface.h"
#include "ray.h"
#include "pixel.h"
#include <algorithm>
#include <emmintrin.h>
#include <memory>

#define PARALLEL

using namespace raytracer;

float computeAvgLuminance(const std::unique_ptr<glm::vec3[]>& buffer, size_t size);
float computeEV100(float aperture, float shutterTime, float ISO);
float computeEV100FromAvgLuminance(float avgLuminance);
float convertEV100ToExposure(float EV100);

glm::vec3 accurateLinearToSRGB(const glm::vec3& linearColor);
glm::vec3 approxLinearToSRGB(const glm::vec3& linearColor);

std::unique_ptr<glm::vec3[]> colour_buffer = nullptr;

void raytracer::raytrace(const Camera& camera, const Scene& scene, Tmpl8::Surface& target_surface) {
	if (colour_buffer == nullptr)
	{
		colour_buffer = std::make_unique<glm::vec3[]>(target_surface.GetWidth() * target_surface.GetHeight());
		// Might be for OpenMP (no false sharing) but requires custom deletion (call FREE64)
		//colour_buffer = std::unique_ptr<glm::vec3[]>(
		//	static_cast<glm::vec3*>(MALLOC64(sizeof(glm::vec3) * target_surface.GetWidth() * target_surface.GetHeight())));
	}

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

	const int width = static_cast<uint>(target_surface.GetWidth());
	const int height = static_cast<uint>(target_surface.GetHeight());
#ifdef PARALLEL
#pragma omp parallel for schedule(dynamic, 45)
#endif
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			glm::vec3 scr_point = scr_base_origin + u_step * (float)x + v_step * (float)y;
			Ray ray(eye, glm::normalize(scr_point - eye));
			colour_buffer[y * width + x] = scene.trace_ray(ray);
		}
	}


	// Usage with manual settings
	float aperture = 1.0f / 5.8f;
	float shutterTime = 1.0f / 100.0f;
	float ISO = 800.0f;
	float EV100 = computeEV100(aperture, shutterTime, ISO);

	// Usage with auto settings
	//float Lavg = computeAvgLuminance(colour_buffer, width * height);
	//float EV100 = computeEV100FromAvgLuminance(Lavg);

	float exposure = convertEV100ToExposure(EV100);


	u32* buffer = target_surface.GetBuffer();
#ifdef PARALLEL
#pragma omp parallel for schedule(dynamic, 45)
#endif
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			glm::vec3 rawColour = colour_buffer[y * width + x];
			glm::vec3 exposedColour = rawColour *exposure;
			glm::vec3 colour = approxLinearToSRGB(exposedColour);

#if TRUE
			PixelRGBA pixel;
			pixel.r = (u8) std::min((u32)(colour.r * 255), 255U);
			pixel.g = (u8) std::min((u32)(colour.g * 255), 255U);
			pixel.b = (u8) std::min((u32)(colour.b * 255), 255U);
			pixel.a = 255;
			buffer[y * width + x] = pixel.value;
#else
			float f_color[4];
			f_color[0] = colour.b;
			f_color[1] = colour.g;
			f_color[2] = colour.r;
			__m128 sse_f_colour = _mm_loadu_ps(f_color);
			sse_f_colour = _mm_mul_ps(sse_f_colour, sse_f_255);// Mul with 255
			__m128i sse_i_colour = _mm_cvtps_epi32(sse_f_colour);// Cast to int
			sse_i_colour = _mm_min_epi32(sse_i_colour, sse_i_255);// Min
			sse_i_colour = _mm_packus_epi32(sse_i_colour, sse_i_colour);// i32 to u16
			sse_i_colour = _mm_packus_epi16(sse_i_colour, sse_i_colour);// u16 to u8
			u8 u8_color[16];
			_mm_storeu_si128((__m128i*)u8_color, sse_i_colour);

			memcpy(&buffer[y * width + x], u8_color, 4);
#endif
		}
	}
}




// https://msdn.microsoft.com/en-us/library/windows/desktop/bb173486(v=vs.85).aspx
float computeAvgLuminance(const std::unique_ptr<glm::vec3[]>& buffer, size_t size)
{
	// Small number to avoid problems with black pixels
	const float delta = 0.0000001f;

	// TODO: parallel (compute average over sections)
	float sum = 0.0f;
	for (uint i = 0; i < size; i++)
	{
		const glm::vec3& colour = buffer[i];
		float luminance = 0.27f * colour.r + 0.67f * colour.g + 0.06f * colour.b;
		sum += log(delta + luminance);
	}

	return exp(1.0f / size * sum);
}

// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr_v2.pdf
float computeEV100(float aperture, float shutterTime, float ISO)
{
	// EV number is defined as:
	// 2^ EV_s = N^2 / t and EV_s = EV_100 + log2 (S /100)
	// This gives
	// EV_s = log2 (N^2 / t)
	// EV_100 + log2 (S /100) = log2 (N^2 / t)
	// EV_100 = log2 (N^2 / t) - log2 (S /100)
	// EV_100 = log2 (N^2 / t . 100 / S)
	return log2(sqrtf(aperture) / shutterTime * 100 / ISO);}

// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr_v2.pdf
float computeEV100FromAvgLuminance(float avgLuminance)
{
	// We later use the middle gray at 12.7% in order to have
	// a middle gray at 18% with a sqrt (2) room for specular highlights
	// But here we deal with the spot meter measuring the middle gray
	// which is fixed at 12.5 for matching standard camera
	// constructor settings (i.e. calibration constant K = 12.5)
	// Reference : http :// en. wikipedia . org / wiki / Film_speed
	return log2(avgLuminance * 100.0f / 12.5f);
}

// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr_v2.pdf
float convertEV100ToExposure(float EV100)
{
	// Compute the maximum luminance possible with H_sbs sensitivity
	// maxLum = 78 / ( S * q ) * N^2 / t
	// = 78 / ( S * q ) * 2^ EV_100
	// = 78 / (100 * 0.65) * 2^ EV_100
	// = 1.2 * 2^ EV
	// Reference : http :// en. wikipedia . org / wiki / Film_speed
	float maxLuminance = 1.2f * pow(2.0f, EV100);	return 1.0f / maxLuminance;
}

// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr_v2.pdf
glm::vec3 accurateLinearToSRGB(const glm::vec3& linearColor)
{
	glm::vec3 sRGBLo = linearColor * 12.92f;
	glm::vec3 sRGBHi = (glm::pow(glm::abs(linearColor), glm::vec3(1.0f / 2.4f)) * 1.055f) - 0.055f;
	//glm::vec3 sRGB = (linearColor <= 0.0031308f) ? sRGBLo : sRGBHi;
	glm::vec3 sRGB;
	sRGB.x = (linearColor.x <= 0.0031308f) ? sRGBLo.x : sRGBHi.x;
	sRGB.y = (linearColor.y <= 0.0031308f) ? sRGBLo.y : sRGBHi.y;
	sRGB.z = (linearColor.z <= 0.0031308f) ? sRGBLo.z : sRGBHi.z;
	return sRGB;}

// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr_v2.pdf
glm::vec3 approxLinearToSRGB(const glm::vec3& linearColor)
{
	const glm::vec3 tmp = glm::vec3(1.0f / 2.2f);
	return glm::pow(linearColor, tmp);
}
