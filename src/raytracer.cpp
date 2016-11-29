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
#include <stdlib.h>
//#include <OpenGL/wglew.h>
#include "template/includes.h"

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
	_helloWorldKernel = LoadKernel("assets/cl/kernel.cl", "hello");
}

raytracer::RayTracer::~RayTracer()
{
}

void raytracer::RayTracer::InitBuffers()
{
	cl_int err;
	// Create output (float) color buffer
	_buffer_size = _scr_width * _scr_height * 3 * sizeof(float);
	_outHost = std::make_unique<float[]>(_scr_width * _scr_height * 3);
	_outDevice = cl::Buffer(_context,
		CL_MEM_WRITE_ONLY,
		_buffer_size,
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");
	if (_num_spheres > 0) {
		_spheres = cl::Buffer(_context,
			CL_MEM_READ_ONLY,
			_num_spheres * sizeof(Sphere),
			NULL,
			&err);
		checkClErr(err, "Buffer::Buffer()");
	}
	if (_num_planes > 0) {
		_planes = cl::Buffer(_context,
			CL_MEM_READ_ONLY,
			_num_planes * sizeof(Plane),
			NULL,
			&err);
		checkClErr(err, "Buffer::Buffer()");
	}
	_materials = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		(_num_mesh_materials + _num_spheres + _num_planes) * sizeof(Material),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");
	_lights = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		_num_lights * sizeof(Light),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");
	_vertices = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		_num_vertices * sizeof(glm::vec4),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");
	_triangles = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		_num_triangles * sizeof(Scene::TriangleSceneData),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");
}

void raytracer::RayTracer::SetScene(const Scene& scene)
{
	_num_spheres = scene.GetSpheres().size();
	_num_planes = scene.GetPlanes().size();
	_num_lights = scene.GetLights().size();
	_num_vertices = scene.GetVertices().size();
	_num_triangles = scene.GetTriangleIndices().size();
	_num_mesh_materials = scene.GetMeshMaterials().size();
	InitBuffers();
	auto& sphereMaterials = scene.GetSphereMaterials();
	auto& planeMaterials = scene.GetPlaneMaterials();
	auto& meshMaterials = scene.GetMeshMaterials();
	std::vector<Material> materials;
	materials.resize(sphereMaterials.size() + planeMaterials.size() + meshMaterials.size());
	
	Material* buffer = materials.data();
	memcpy(buffer, sphereMaterials.data(), sphereMaterials.size() * sizeof(Material));
	buffer += sphereMaterials.size();
	memcpy(buffer, planeMaterials.data(), planeMaterials.size() * sizeof(Material));
	buffer += planeMaterials.size();
	memcpy(buffer, meshMaterials.data(), meshMaterials.size() * sizeof(Material));
	
	std::cout << "sphere materials: " << sphereMaterials.size() << std::endl;
	std::cout << "plane materials: " << planeMaterials.size() << std::endl;
	std::cout << "mesh materials: " << meshMaterials.size() << std::endl;
	std::cout << "total number of materials: " << materials.size() << std::endl;

	cl_int err;
	if (_num_spheres > 0) {
		err = _queue.enqueueWriteBuffer(
			_spheres,
			CL_TRUE,
			0,
			_num_spheres * sizeof(Sphere),
			scene.GetSpheres().data());
		checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	}
	if (_num_planes > 0) {
		err = _queue.enqueueWriteBuffer(
			_planes,
			CL_TRUE,
			0,
			_num_planes * sizeof(Plane),
			scene.GetPlanes().data());
		checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	}	
	err = _queue.enqueueWriteBuffer(
		_materials,
		CL_TRUE,
		0,
		materials.size() * sizeof(Material),
		materials.data());
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	err = _queue.enqueueWriteBuffer(
		_lights,
		CL_TRUE,
		0,
		_num_lights * sizeof(Light),
		scene.GetLights().data());
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	err = _queue.enqueueWriteBuffer(
		_vertices,
		CL_TRUE,
		0,
		_num_vertices * sizeof(glm::vec4),
		scene.GetVertices().data());
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");
	err = _queue.enqueueWriteBuffer(
		_triangles,
		CL_TRUE,
		0,
		_num_triangles * sizeof(Scene::TriangleSceneData),
		scene.GetTriangleIndices().data());
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
	_helloWorldKernel.setArg(1, _scr_width);
	_helloWorldKernel.setArg(2, glmToCl(eye));
	_helloWorldKernel.setArg(3, glmToCl(scr_base_origin));
	_helloWorldKernel.setArg(4, glmToCl(u_step));
	_helloWorldKernel.setArg(5, glmToCl(v_step));
	_helloWorldKernel.setArg(6, _num_spheres);// Num spheres
	_helloWorldKernel.setArg(7, _spheres);
	_helloWorldKernel.setArg(8, _num_planes);// Num spheres
	_helloWorldKernel.setArg(9, _planes);
	_helloWorldKernel.setArg(10, _num_vertices);
	_helloWorldKernel.setArg(11, _vertices);
	_helloWorldKernel.setArg(12, _num_triangles);
	_helloWorldKernel.setArg(13, _triangles);
	_helloWorldKernel.setArg(14, _materials);
	_helloWorldKernel.setArg(15, _num_lights);
	_helloWorldKernel.setArg(16, _lights);
	err = _queue.enqueueNDRangeKernel(
		_helloWorldKernel,
		cl::NullRange,
		cl::NDRange(_scr_width, _scr_height),
		cl::NullRange,
		NULL,
		&event);
	checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");

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
