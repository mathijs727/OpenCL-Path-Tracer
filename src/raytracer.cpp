#include "raytracer.h"

#include "camera.h"
#include "scene.h"
#include "template/surface.h"
#include "ray.h"
#include "pixel.h"
#include "Texture.h"
#include <algorithm>
#include <iostream>
#include <emmintrin.h>
#include <vector>
#include <fstream>
#include <utility>
#include <thread>
#include <stdlib.h>
#include <chrono>
//#include <OpenGL/wglew.h>
#include "template/includes.h"
#include "allocator_singletons.h"

struct KernelData
{
	// Camera
	cl_float3 eye;// Position of the camera "eye"
	cl_float3 screen;// Left top of screen in world space
	cl_float3 u_step;// Horizontal distance between pixels in world space
	cl_float3 v_step;// Vertical distance between pixels in world space
	uint width;// Render target width

	// Scene
	int numVertices, numTriangles, numLights;

	int topLevelBvhRoot;
};


#ifdef WIN32
int setenv(const char *name, const char *value, int overwrite) {
	int errcode = 0;
	if (!overwrite) {
		size_t envsize = 0;
		errcode = getenv_s(&envsize, NULL, 0, name);
		if (errcode || envsize) return errcode;
	}
	return _putenv_s(name, value);
}
#endif

using namespace raytracer;

cl_float3 glmToCl(const glm::vec3& vec);
//void floatToPixel(float* floats, uint32_t* pixels, int count);

// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
#ifdef _WIN32

#ifdef _DEBUG
#define checkClErr(ERROR_CODE, NAME) \
	if ((ERROR_CODE) != CL_SUCCESS) { \
		std::cout << "OpenCL ERROR: " << NAME << " " << (ERROR_CODE) << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
		system("PAUSE"); \
		exit(EXIT_FAILURE); \
	}
#else
#define checkClErr(ERROR_CODE, NAME)
#endif

#else
#define checkClErr(ERROR_CODE, NAME) \
	if ((ERROR_CODE) != CL_SUCCESS) { \
		std::cout << "OpenCL ERROR: " << NAME << " " << (ERROR_CODE) << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
		exit(EXIT_FAILURE); \
	}
#endif

raytracer::RayTracer::RayTracer(int width, int height)
{
	_scr_width = width;
	_scr_height = height;

	InitOpenCL();
	_helloWorldKernel = LoadKernel("assets/cl/kernel.cl", "hello");
}

raytracer::RayTracer::~RayTracer()
{
}

void raytracer::RayTracer::InitBuffers()
{
	cl_int err;

	_materials = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(_num_mesh_materials, 1) * sizeof(Material),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_lights = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1, _num_lights) * sizeof(Light),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_vertices = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1, _num_vertices) * sizeof(VertexSceneData),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_triangles = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1, _num_triangles) * sizeof(TriangleSceneData),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_sub_bvh = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1, _num_sub_bvh_nodes) * sizeof(SubBvhNode),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_num_top_bvh_nodes[0] = 0;
	_top_bvh[0] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		256 * sizeof(TopBvhNode),// TODO: Make this dynamic so we dont have a fixed max of 256 top level BVH nodes.
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_num_top_bvh_nodes[1] = 0;
	_top_bvh[1] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		256 * sizeof(TopBvhNode),// TODO: Make this dynamic so we dont have a fixed max of 256 top level BVH nodes.
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_kernel_data = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		sizeof(KernelData),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	// https://www.khronos.org/registry/cl/specs/opencl-cplusplus-1.2.pdf
	_material_textures = cl::Image2DArray(_context,
		CL_MEM_READ_ONLY,
		cl::ImageFormat(CL_BGRA, CL_UNORM_INT8),
		std::max(1, _num_textures),
		Texture::TEXTURE_WIDTH,
		Texture::TEXTURE_HEIGHT,
		0, 0, NULL,// Unused host_ptr
		&err);
	checkClErr(err, "cl::Image2DArray");
}

void raytracer::RayTracer::SetScene(Scene& scene)
{
	using namespace std::chrono;

	// Build BVH and move it to OpenCL
	/*std::cout << "Building BVH" << std::endl;
	_bvh = std::unique_ptr<Bvh>(new Bvh(scene));
	auto t1 = high_resolution_clock::now();
	_bvh->buildsubBvhs();
	auto t2 = high_resolution_clock::now();
	auto d = duration_cast<duration<double>>(t2 - t1);
	std::cout << "sub BVH construction time: " << d.count() * 1000.0 << "ms" << std::endl;

	std::cout << "Done building BVH" << std::endl;*/
	_bvhBuilder = std::make_unique<TopLevelBvhBuilder>(scene);

	auto& vertices = getVertexAllocatorInstance();
	auto& triangles = getTriangleAllocatorInstance();
	auto& materials = getMaterialAllocatorInstance();
	auto& subBvhNodes = getSubBvhAllocatorInstance();

	_num_lights = scene.GetLights().size();
	_num_vertices = vertices.size();
	_num_triangles = triangles.size();
	_num_mesh_materials = materials.size();
	_num_textures = Texture::getNumUniqueSurfaces();
	_num_sub_bvh_nodes = subBvhNodes.size();

	InitBuffers();

	cl_int err;
	// Copy textures to the GPU
	for (uint texId = 0; texId < _num_textures; texId++)
	{
		Tmpl8::Surface* surface = Texture::getSurface(texId);

		// Origin (o) and region (r)
		cl::size_t<3> o; o[0] = 0; o[1] = 0; o[2] = texId;

		cl::size_t<3> r;
		r[0] = Texture::TEXTURE_WIDTH;
		r[1] = Texture::TEXTURE_HEIGHT;
		r[2] = 1;// r[2] must be 1?
		err = _queue.enqueueWriteImage(
			_material_textures,
			CL_TRUE,
			o,
			r,
			0,
			0,
			surface->GetBuffer());
		checkClErr(err, "CommandQueue::enqueueWriteImage");
	}

	if (_num_mesh_materials > 0)
	{
		err = _queue.enqueueWriteBuffer(
			_materials,
			CL_TRUE,
			0,
			_num_mesh_materials * sizeof(Material),
			materials.data());
		checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	}

	if (_num_lights > 0)
	{
		for (auto light : scene.GetLights())
		{
			if (light.type == Light::Type::Point)
			{
				std::cout << "Light atten radius:" << light.point.sqrAttRadius << std::endl;
			}
		}
		err = _queue.enqueueWriteBuffer(
			_lights,
			CL_TRUE,
			0,
			_num_lights * sizeof(Light),
			scene.GetLights().data());
		checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	}

	if (_num_vertices > 0)
	{
		err = _queue.enqueueWriteBuffer(
			_vertices,
			CL_TRUE,
			0,
			_num_vertices * sizeof(VertexSceneData),
			vertices.data());
		checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	}

	if (_num_triangles > 0)
	{
		err = _queue.enqueueWriteBuffer(
			_triangles,
			CL_TRUE,
			0,
			_num_triangles * sizeof(TriangleSceneData),
			triangles.data());
		checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	}

	if (_num_sub_bvh_nodes > 0)
	{
		err = _queue.enqueueWriteBuffer(
			_sub_bvh,
			CL_TRUE,
			0,
			_num_sub_bvh_nodes * sizeof(SubBvhNode),
			subBvhNodes.data());
		checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	}
}

void raytracer::RayTracer::SetTarget(GLuint glTexture)
{
	cl_int err;
	_output_image = cl::ImageGL(_context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, glTexture, &err);
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

	{
		// Copy camera (and scene) data to the device using a struct so we dont use 20 kernel arguments
		KernelData data = {};
		data.eye = glmToCl(eye);
		data.screen = glmToCl(scr_base_origin);
		data.u_step = glmToCl(u_step);
		data.v_step = glmToCl(v_step);
		data.width = _scr_width;

		data.numVertices = _num_vertices;
		data.numTriangles = _num_triangles;
		data.numLights = _num_lights;

		data.topLevelBvhRoot = _num_top_bvh_nodes[_active_top_bvh] - 1;

		cl_int err = _queue.enqueueWriteBuffer(
			_kernel_data,
			CL_TRUE,
			0,
			sizeof(KernelData),
			&data);
		checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	}

	std::vector<cl::Memory> images = { _output_image };
	_queue.enqueueAcquireGLObjects(&images);

	cl_int err;
	cl::Event event;
	_helloWorldKernel.setArg(0, _output_image);
	_helloWorldKernel.setArg(1, _kernel_data);
	_helloWorldKernel.setArg(2, _vertices);
	_helloWorldKernel.setArg(3, _triangles);
	_helloWorldKernel.setArg(4, _materials);
	_helloWorldKernel.setArg(5, _material_textures);
	_helloWorldKernel.setArg(6, _lights);
	_helloWorldKernel.setArg(7, _sub_bvh);
	_helloWorldKernel.setArg(8, _top_bvh[_active_top_bvh]);

	err = _queue.enqueueNDRangeKernel(
		_helloWorldKernel,
		cl::NullRange,
		cl::NDRange(_scr_width, _scr_height),
		cl::NullRange,
		NULL,
		&event);
	checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");
	
	{
		// Manually flush the queue
		// At least on AMD, the queue is flushed after the enqueueWriteBuffer (probably because it subks
		//  one kernel launch is not enough reason to flush). So it would be executed at _queue.finish(), which
		//  is called after the top lvl bvh construction (which is expensive) has completed. Instead we manually
		//  flush and than calculate the top lvl bvh.
		_queue.flush();

		// Update the top level BVH and copy it to the GPU on a seperate copy queue
		auto& topNodes = getTopBvhAllocatorInstance();
		topNodes.clear();
		_bvhBuilder->build();


		int copyBvh = (_active_top_bvh + 1) % 2;
		_num_top_bvh_nodes[copyBvh] = topNodes.size();
		if (_num_top_bvh_nodes[copyBvh] > 0)
		{
			cl::Event copyEvent;
			cl_int err = _copyQueue.enqueueWriteBuffer(
				_top_bvh[copyBvh],
				CL_TRUE,// TODO: can we make this "false" again?
				0,
				_num_top_bvh_nodes[copyBvh] * sizeof(TopBvhNode),
				topNodes.data(),
				nullptr,
				&copyEvent);
			checkClErr(err, "CommandQueue::enqueueWriteBuffer");

			std::vector<cl::Event> waitEvents;
			waitEvents.push_back(copyEvent);
			err = _queue.enqueueBarrierWithWaitList(&waitEvents);
			checkClErr(err, "CommandQueue::enqueueBarrierWithWaitList");
		}
	}

	// Before returning the objects to OpenGL, we sync to make sure OpenCL is done.
	err = _queue.finish();
	checkClErr(err, "CommandQueue::finish");

	_queue.enqueueReleaseGLObjects(&images);

	_active_top_bvh = (_active_top_bvh + 1) % 2;
}

// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
// https://www.codeproject.com/articles/685281/opengl-opencl-interoperability-a-case-study-using
void raytracer::RayTracer::InitOpenCL()
{
	setenv("CUDA_CACHE_DISABLE", "1", 1);
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
	_copyQueue = clCreateCommandQueue(lContext, lDeviceId, 0, &lError);
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
