#include "raytracer.h"

#include "camera.h"
#include "scene.h"
#include "template/surface.h"
#include "ray.h"
#include "pixel.h"
#include <algorithm>
#include <iostream>
#include <emmintrin.h>
#include <vector>
#include <fstream>
#include <utility>

using namespace raytracer;

const std::string hw("Hello World\n");

// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
inline void checkClErr(cl_int err, const char* name)
{
	if (err != CL_SUCCESS) {
		std::cout << "OpenCL ERROR: " << name << " (" << err << ")" << std::endl;
		system("PAUSE");
		exit(EXIT_FAILURE);
	}
}

raytracer::RayTracer::RayTracer()
{
	InitOpenCL();
	InitBuffers();
	_helloWorldKernel = LoadKernel("assets/cl/kernel.cl", "hello");
}

raytracer::RayTracer::~RayTracer()
{
}

void raytracer::RayTracer::InitBuffers()
{
	cl_int err;

	_outH = std::make_unique<char[]>(hw.length() + 1);
	_outCL = cl::Buffer(_context,
		CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
		hw.length() + 1,
		_outH.get(),
		&err);
	checkClErr(err, "Buffer::Buffer()");
}

void raytracer::RayTracer::DoSomething()
{
	cl_int err;
	cl::Event event;
	_helloWorldKernel.setArg(0, _outCL);
	err = _queue.enqueueNDRangeKernel(
		_helloWorldKernel,
		cl::NullRange,
		cl::NDRange(hw.length() + 1),
		cl::NDRange(1, 1),
		NULL,
		&event);
	checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");

	event.wait();
	err = _queue.enqueueReadBuffer(
		_outCL,
		CL_TRUE,
		0,
		hw.length() + 1,
		_outH.get());
	checkClErr(err, "CommandQueue::enqueueReadBuffer");
	std::cout << "OpenCL result: " << _outH.get() << std::endl;
}

// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
void raytracer::RayTracer::InitOpenCL()
{
	cl_int err;
	std::vector<cl::Platform> platformList;
	cl::Platform::get(&platformList);
	checkClErr(platformList.size() != 0 ? CL_SUCCESS : -1, "cl::Platform::get");
	std::cout << "Platform number is: " << platformList.size() << std::endl;
	
	std::string platformVendor;
	platformList[0].getInfo((cl_platform_info)CL_PLATFORM_VENDOR, &platformVendor);
	std::cout << "Platform is by: " << platformVendor << std::endl;
	
	cl_context_properties cprops[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties)(platformList[0])(), 0 };
	_context = cl::Context(CL_DEVICE_TYPE_CPU,
		cprops,
		NULL,
		NULL,
		&err);
	checkClErr(err, "Context::Context");

	_devices = _context.getInfo<CL_CONTEXT_DEVICES>();
	checkClErr(_devices.size() > 0 ? CL_SUCCESS : -1, "devices.size() > 0");

	_queue = cl::CommandQueue(_context, _devices[0], 0, &err);
	checkClErr(err, "CommandQueue::CommandQueue()");
}

cl::Kernel raytracer::RayTracer::LoadKernel(const char* fileName, const char* funcName)
{
	cl_int err;

	std::ifstream file(fileName);
	{
		std::string errorMessage = "Cannot open file: ";
		errorMessage += fileName;
		checkClErr(file.is_open() ? CL_SUCCESS : -1, errorMessage.c_str());
	}

	std::string prog(std::istreambuf_iterator<char>(file),
		(std::istreambuf_iterator<char>()));
	cl::Program::Sources source(1, std::make_pair(prog.c_str(), prog.length()+1));
	cl::Program program(_context, source);
	err = program.build(_devices, "");
	{
		std::string errorMessage = "Cannot build program: ";
		errorMessage += fileName;
		checkClErr(err, errorMessage.c_str());
	}

	cl::Kernel kernel(program, funcName, &err);
	{
		std::string errorMessage = "Cannot create kernel: ";
		errorMessage += fileName;
		checkClErr(err, errorMessage.c_str());
	}

	return kernel;
}








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
#pragma omp parallel for schedule(dynamic, 45)
	for (int y = 0; y < target_surface.GetHeight(); ++y) {
		for (int x = 0; x < target_surface.GetWidth(); ++x) {
			glm::vec3 scr_point = scr_base_origin + u_step * (float)x + v_step * (float)y;
			Ray ray(eye, glm::normalize(scr_point - eye));
			glm::vec3 colour = scene.trace_ray(ray);

#if FALSE
			PixelRGBA pixel;
			pixel.r = (u8)std::min((u32)(colour.r * 255), 255U);
			pixel.g = (u8)std::min((u32)(colour.g * 255), 255U);
			pixel.b = (u8)std::min((u32)(colour.b * 255), 255U);
			pixel.a = 255;
			*buffer = pixel.value;
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

			memcpy(&buffer[y * target_surface.GetWidth() + x], u8_color, 4);
#endif
		}
	}
}