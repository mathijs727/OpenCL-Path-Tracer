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
#include <thread>
//#include <OpenGL/wglew.h>
#include "template/includes.h"

using namespace raytracer;

cl_float3 glmToCl(const glm::vec3& vec);
//void floatToPixel(float* floats, uint32_t* pixels, int count);

// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
inline void checkClErr(cl_int err, const char* name)
{
	if (err != CL_SUCCESS) {
		std::cout << "OpenCL ERROR: " << name << " (" << err << ")" << std::endl;
#ifdef _WIN32
		system("PAUSE");
#endif
		exit(EXIT_FAILURE);
	}
}

raytracer::RayTracer::RayTracer(int width, int height)
{
	_scr_width = width;
	_scr_height = height;

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

	{
		// Create output (float) color buffer
		_buffer_size = _scr_width * _scr_height * 3 * sizeof(float);
		_outHost = std::make_unique<float[]>(_scr_width * _scr_height * 3);
		_outDevice = cl::Buffer(_context,
			CL_MEM_WRITE_ONLY,
			_buffer_size,
			NULL,
			&err);
		checkClErr(err, "Buffer::Buffer()");
	}

	{
		_spheres = cl::Buffer(_context,
			CL_MEM_READ_ONLY,
			128 * sizeof(Sphere),
			NULL,
			&err);
		checkClErr(err, "Buffer::Buffer()");
	}

	{
		_planes = cl::Buffer(_context,
			CL_MEM_READ_ONLY,
			128 * sizeof(Plane),
			NULL,
			&err);
		checkClErr(err, "Buffer::Buffer()");
	}

	{
		_materials = cl::Buffer(_context,
			CL_MEM_READ_ONLY,
			256 * sizeof(SerializedMaterial),
			NULL,
			&err);
		checkClErr(err, "Buffer::Buffer()");
	}

	{
		_lights = cl::Buffer(_context,
			CL_MEM_READ_ONLY,
			16 * sizeof(Light),
			NULL,
			&err);
		checkClErr(err, "Buffer::Buffer()");
	}
}

void raytracer::RayTracer::SetScene(const Scene& scene)
{
	auto spheres = std::make_unique<Sphere[]>(128);
	auto planes = std::make_unique<Plane[]>(128);
	auto materials = std::make_unique<SerializedMaterial[]>(256);
	auto lights = std::make_unique<Light[]>(16);

	_num_spheres = scene.GetSpheres().size();
	_num_planes = scene.GetPlanes().size();
	memcpy(spheres.get(), scene.GetSpheres().data(), _num_spheres * sizeof(Sphere));
	memcpy(planes.get(), scene.GetPlanes().data(), _num_planes * sizeof(Plane));

	/*memcpy(materials.get(), scene.GetSphereMaterials().data(), _num_spheres * sizeof(SerializedMaterial));
	memcpy(materials.get() + _num_spheres,
		scene.GetPlaneMaterials().data(),
		_num_planes * sizeof(Material));*/
	auto sphereMaterials = scene.GetSphereMaterials();
	auto planeMaterials = scene.GetPlaneMaterials();
	int i = 0;
	for (const Material& mat : sphereMaterials)
	{
		materials[i++] = mat;//SerializedMaterial(mat);
	}
	for (const Material& mat : planeMaterials)
	{
		materials[i++] = mat;//SerializedMaterial(mat);
	}

	_num_lights = scene.GetLights().size();
	memcpy(lights.get(), scene.GetLights().data(), _num_lights * sizeof(Light));

	cl_int err;
	err = _queue.enqueueWriteBuffer(
		_spheres,
		CL_TRUE,
		0,
		128 * sizeof(Sphere),
		spheres.get());
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");

	err = _queue.enqueueWriteBuffer(
		_planes,
		CL_TRUE,
		0,
		128 * sizeof(Plane),
		planes.get());
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");

	err = _queue.enqueueWriteBuffer(
		_materials,
		CL_TRUE,
		0,
		256 * sizeof(SerializedMaterial),
		materials.get());
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");

	err = _queue.enqueueWriteBuffer(
		_lights,
		CL_TRUE,
		0,
		16 * sizeof(Light),
		lights.get());
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");
}

void raytracer::RayTracer::SetTarget(GLuint glTexture)
{
	cl_int err;
	_outputImage = cl::ImageGL(_context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, glTexture, &err);
	checkClErr(err, "ImageGL");
}

void raytracer::RayTracer::RayTrace(const Camera& camera)
{
	glm::vec3 eye;
	glm::vec3 scr_base_origin;
	glm::vec3 scr_base_u;
	glm::vec3 scr_base_v;
	camera.get_frustum(eye, scr_base_origin, scr_base_u, scr_base_v);

	glm::vec3 u_step = scr_base_u / (float)_scr_width;
	glm::vec3 v_step = scr_base_v / (float)_scr_height;

	// We must make sure that OpenGL is done with the textures, so we ask to sync.
	glFinish();

	std::vector<cl::Memory> images = { _outputImage };
	_queue.enqueueAcquireGLObjects(&images);

	cl_int err;
	cl::Event event;
	_helloWorldKernel.setArg(0, _outputImage);
	_helloWorldKernel.setArg(1, (cl_uint)_scr_width);
	_helloWorldKernel.setArg(2, glmToCl(eye));
	_helloWorldKernel.setArg(3, glmToCl(scr_base_origin));
	_helloWorldKernel.setArg(4, glmToCl(u_step));
	_helloWorldKernel.setArg(5, glmToCl(v_step));
	_helloWorldKernel.setArg(6, _num_spheres);// Num spheres
	_helloWorldKernel.setArg(7, _spheres);
	_helloWorldKernel.setArg(8, _num_planes);// Num spheres
	_helloWorldKernel.setArg(9, _planes);
	_helloWorldKernel.setArg(10, _materials);
	_helloWorldKernel.setArg(11, _num_lights);
	_helloWorldKernel.setArg(12, _lights);
	err = _queue.enqueueNDRangeKernel(
		_helloWorldKernel,
		cl::NullRange,
		cl::NDRange(_scr_width, _scr_height),
		cl::NullRange,
		NULL,
		&event);
	checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");

	event.wait();
	err = _queue.enqueueReadBuffer(
		_outDevice,
		CL_TRUE,
		0,
		_buffer_size,
		_outHost.get());
	checkClErr(err, "CommandQueue::enqueueReadBuffer");

	// Before returning the objects to OpenGL, we sync to make sure OpenCL is done.
	err = _queue.finish();
	checkClErr(err, "CommandQueue::finish");

	_queue.enqueueReleaseGLObjects(&images);
	//floatToPixel(_outHost.get(), target_surface.GetBuffer(), _scr_width * _scr_height);
}

// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
// https://www.codeproject.com/articles/685281/opengl-opencl-interoperability-a-case-study-using
void raytracer::RayTracer::InitOpenCL()
{
	cl_int lError = CL_SUCCESS;
	std::string lBuffer;

	//
	// Generic OpenCL creation.
	//

	// Get platforms.
	cl_uint lNbPlatformId = 0;
	clGetPlatformIDs(0, 0, &lNbPlatformId);

	if (lNbPlatformId == 0)
	{
		std::cout << "Unable to find an OpenCL platform." << std::endl;
#ifdef _WIN32
		system("PAUSE");
#endif	
		exit(EXIT_FAILURE);
	}


	// Loop on all platforms.
	std::vector< cl_platform_id > lPlatformIds(lNbPlatformId);
	clGetPlatformIDs(lNbPlatformId, lPlatformIds.data(), 0);

	// Try to find the device with the compatible context.
	cl_platform_id lPlatformId = 0;
	cl_device_id lDeviceId = 0;
	cl_context lContext = 0;

	for (size_t i = 0; i < lPlatformIds.size() && lContext == 0; ++i)
	{
		const cl_platform_id lPlatformIdToTry = lPlatformIds[i];

		// Get devices.
		cl_uint lNbDeviceId = 0;
		clGetDeviceIDs(lPlatformIdToTry, CL_DEVICE_TYPE_GPU, 0, 0, &lNbDeviceId);

		if (lNbDeviceId == 0)
		{
			continue;
		}

		std::vector< cl_device_id > lDeviceIds(lNbDeviceId);
		clGetDeviceIDs(lPlatformIdToTry, CL_DEVICE_TYPE_GPU, lNbDeviceId, lDeviceIds.data(), 0);


		// Create the properties for this context.
		cl_context_properties lContextProperties[] = {
			// We need to add information about the OpenGL context with
			// which we want to exchange information with the OpenCL context.
#if defined (WIN32)
			// We should first check for cl_khr_gl_sharing extension.
			CL_GL_CONTEXT_KHR , (cl_context_properties)wglGetCurrentContext() ,
			CL_WGL_HDC_KHR , (cl_context_properties)wglGetCurrentDC() ,
#elif defined (__linux__)
			// We should first check for cl_khr_gl_sharing extension.
			CL_GL_CONTEXT_KHR , (cl_context_properties)glXGetCurrentContext() ,
			CL_GLX_DISPLAY_KHR , (cl_context_properties)glXGetCurrentDisplay() ,
#elif defined (__APPLE__)
			// We should first check for cl_APPLE_gl_sharing extension.
#if 0
			// This doesn't work.
			CL_GL_CONTEXT_KHR , (cl_context_properties)CGLGetCurrentContext() ,
			CL_CGL_SHAREGROUP_KHR , (cl_context_properties)CGLGetShareGroup(CGLGetCurrentContext()) ,
#else
			CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE , (cl_context_properties)CGLGetShareGroup(CGLGetCurrentContext()) ,
#endif
#endif
			CL_CONTEXT_PLATFORM , (cl_context_properties)lPlatformIdToTry ,
			0 , 0 ,
		};


		// Look for the compatible context.
		for (size_t j = 0; j < lDeviceIds.size(); ++j)
		{
			cl_device_id lDeviceIdToTry = lDeviceIds[j];
			cl_context lContextToTry = 0;

			lContextToTry = clCreateContext(
				lContextProperties,
				1, &lDeviceIdToTry,
				0, 0,
				&lError
			);
			if (lError == CL_SUCCESS)
			{
				// We found the context.
				lPlatformId = lPlatformIdToTry;
				lDeviceId = lDeviceIdToTry;
				lContext = lContextToTry;
				break;
			}
		}
	}

	if (lDeviceId == 0)
	{
		std::cout << "Unable to find a compatible OpenCL device." << std::endl;
#ifdef _WIN32
		system("PAUSE");
#endif
		exit(EXIT_FAILURE);
	}


	// Create a command queue.
	_context = lContext;
	_devices.push_back(cl::Device(lDeviceId));
	_queue = clCreateCommandQueue(lContext, lDeviceId, 0, &lError);
	checkClErr(lError, "Unable to create an OpenCL command queue.");
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
	err = program.build(_devices, "-I assets/cl/");
	{
		if (err != CL_SUCCESS)
		{
			std::string errorMessage = "Cannot build program: ";
			errorMessage += fileName;
			std::cout << "Cannot build program: " << fileName << std::endl;

			std::string error;
			program.getBuildInfo(_devices[0], CL_PROGRAM_BUILD_LOG, &error);
			std::cout << error << std::endl;

#ifdef _WIN32
			system("PAUSE");
#endif
			exit(EXIT_FAILURE);
		}
	}

	cl::Kernel kernel(program, funcName, &err);
	{
		std::string errorMessage = "Cannot create kernel: ";
		errorMessage += fileName;
		checkClErr(err, errorMessage.c_str());
	}

	return kernel;
}

cl_float3 glmToCl(const glm::vec3 & vec)
{
	return { vec.x, vec.y, vec.z };
}

/*void floatToPixel(float* floats, uint32_t* pixels, int count)
{
	for (int i = 0; i < count; i++)
	{
		float r = floats[i * 3 + 0];
		float g = floats[i * 3 + 1];
		float b = floats[i * 3 + 2];

		PixelRGBA pixel;
		pixel.r = (u8)std::min((u32)(r * 255), 255U);
		pixel.g = (u8)std::min((u32)(g * 255), 255U);
		pixel.b = (u8)std::min((u32)(b * 255), 255U);
		pixel.a = 255;
		pixels[i] = pixel.value;
	}
}*/











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