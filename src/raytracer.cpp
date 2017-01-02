#include "raytracer.h"

#include "camera.h"
#include "scene.h"
#include "template/surface.h"
#include "ray.h"
#include "pixel.h"
#include "Texture.h"
#include "template/includes.h"
#include <algorithm>
#include <iostream>
#include <emmintrin.h>
#include <vector>
#include <fstream>
#include <utility>
#include <thread>
#include <stdlib.h>
#include <chrono>
#include <clRNG\mrg31k3p.h>

//#define PROFILE_OPENCL
#define MAX_RAYS_PER_PIXEL 10000
#define RAYS_PER_PASS 10

struct KernelData
{
	// Camera
	cl_float3 eye;// Position of the camera "eye"
	cl_float3 screen;// Left top of screen in world space
	cl_float3 u_step;// Horizontal distance between pixels in world space
	cl_float3 v_step;// Vertical distance between pixels in world space
	uint width;// Render target width

	// Scene
	uint numVertices, numTriangles, numEmmisiveTriangles, numLights;
	uint topLevelBvhRoot;

	uint raysPerPass;
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



template<typename T>
inline void writeToBuffer(
	cl::CommandQueue& queue,
	cl::Buffer& buffer,
	const std::vector<T>& items,
	size_t offset = 0)
{
	if (items.size() == 0)
		return;

	cl_int err = queue.enqueueWriteBuffer(
		buffer,
		CL_TRUE,
		offset * sizeof(T),
		(items.size() - offset) * sizeof(T),
		&items[offset]);
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");
}

template<typename T>
inline void writeToBuffer(
	cl::CommandQueue& queue,
	cl::Buffer& buffer,
	const std::vector<T>& items,
	size_t offset,
	std::vector<cl::Event>& events)
{
	if (items.size() == 0)
		return;

	cl::Event ev;
	cl_int err = queue.enqueueWriteBuffer(
		buffer,
		CL_TRUE,
		offset * sizeof(T),
		(items.size() - offset) * sizeof(T),
		&items[offset],
		nullptr,
		&ev);
	events.push_back(ev);
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");
}

inline void timeOpenCL(cl::Event& ev, const char* operationName)
{
#ifdef PROFILE_OPENCL
	ev.wait();
	cl_ulong startTime, stopTime;
	ev.getProfilingInfo(CL_PROFILING_COMMAND_START, &startTime);
	ev.getProfilingInfo(CL_PROFILING_COMMAND_END, &stopTime);
	double totalTime = stopTime - startTime;
	std::cout << "Timing (" << operationName << "): " << (totalTime / 1000000.0) << "ms" << std::endl;
#endif
}




raytracer::RayTracer::RayTracer(int width, int height) : _rays_per_pixel(0)
{
	_scr_width = width;
	_scr_height = height;

	InitOpenCL();
	_ray_trace_kernel = LoadKernel("assets/cl/kernel.cl", "traceRays");
	_accumulate_kernel = LoadKernel("assets/cl/accumulate.cl", "accumulate");

	_top_bvh_root_node[0] = 0;
	_top_bvh_root_node[1] = 0;
}

raytracer::RayTracer::~RayTracer()
{
}

void raytracer::RayTracer::InitBuffers(
	u32 numVertices,
	u32 numTriangles,
	u32 numEmmisiveTriangles,
	u32 numMaterials,
	u32 numSubBvhNodes,
	u32 numTopBvhNodes,
	u32 numLights)
{
	cl_int err;

	_vertices[0] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numVertices) * sizeof(VertexSceneData),
		NULL,
		&err);
	_vertices[1] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numVertices) * sizeof(VertexSceneData),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_triangles[0] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numTriangles) * sizeof(TriangleSceneData),
		NULL,
		&err);
	_triangles[1] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numTriangles) * sizeof(TriangleSceneData),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_emmisive_trangles[0] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numEmmisiveTriangles) * sizeof(u32),
		NULL,
		&err);
	_emmisive_trangles[1] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numEmmisiveTriangles) * sizeof(u32),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_materials[0] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numMaterials) * sizeof(Material),
		NULL,
		&err);
	_materials[1] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numMaterials) * sizeof(Material),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_sub_bvh[0] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numSubBvhNodes) * sizeof(SubBvhNode),
		NULL,
		&err);
	_sub_bvh[1] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numSubBvhNodes) * sizeof(SubBvhNode),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_top_bvh[0] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		numTopBvhNodes * sizeof(TopBvhNode),// TODO: Make this dynamic so we dont have a fixed max of 256 top level BVH nodes.
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");
	_top_bvh[1] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		numTopBvhNodes * sizeof(TopBvhNode),// TODO: Make this dynamic so we dont have a fixed max of 256 top level BVH nodes.
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	_lights = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numLights) * sizeof(Light),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	// https://www.khronos.org/registry/cl/specs/opencl-cplusplus-1.2.pdf
	_material_textures = cl::Image2DArray(_context,
		CL_MEM_READ_ONLY,
		cl::ImageFormat(CL_BGRA, CL_UNORM_INT8),
		std::max((u32)1u, (u32)Texture::getNumUniqueSurfaces()),
		Texture::TEXTURE_WIDTH,
		Texture::TEXTURE_HEIGHT,
		0, 0, NULL,// Unused host_ptr
		&err);
	checkClErr(err, "cl::Image2DArray");




	_ray_kernel_data = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		sizeof(KernelData),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	// Create random streams and copy them to the GPU
	size_t numWorkItems = _scr_width * _scr_height;
	size_t streamBufferSize;
	clrngMrg31k3pStream* streams = clrngMrg31k3pCreateStreams(
		NULL, numWorkItems, &streamBufferSize, (clrngStatus*)&err);
	checkClErr(err, "clrngMrg31k3pCreateStreams");
	_random_streams = cl::Buffer(_context,
		CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		streamBufferSize,
		streams,
		&err);
	checkClErr(err, "Buffer::Buffer()");
	clrngMrg31k3pDestroyStreams(streams);
	



	_accumulation_buffer = cl::Buffer(_context,
		CL_MEM_READ_WRITE,
		_scr_width * _scr_height * sizeof(cl_float3),
		nullptr,
		&err);
	checkClErr(err, "cl::Buffer");
}

void raytracer::RayTracer::SetScene(std::shared_ptr<Scene> scene)
{
	_scene = scene;

	// Initialize buffers
	_num_static_vertices = 0;
	_num_static_triangles = 0;
	_num_static_emmisive_triangles = 0;
	_num_static_materials = 0;
	_num_static_bvh_nodes = 0;
	_num_lights = (u32)scene->get_lights().size();

	u32 numVertices = 0;
	u32 numTriangles= 0;
	u32 numEmmisiveTriangles = 0;
	u32 numMaterials = 0;
	u32 numBvhNodes = 0;
	for (auto& meshBvhPair : scene->get_meshes())
	{
		auto mesh = meshBvhPair.mesh;

		if (mesh->isDynamic())
		{
			numVertices += mesh->maxNumVertices();
			numTriangles += mesh->maxNumTriangles();
			numMaterials += mesh->maxNumMaterials();
			numBvhNodes += mesh->maxNumBvhNodes();
		}
		else {
			numVertices += (u32)mesh->getVertices().size();
			numTriangles += (u32)mesh->getTriangles().size();
			numEmmisiveTriangles += (u32)mesh->getEmmisiveTriangles().size();
			numMaterials += (u32)mesh->getMaterials().size();
			numBvhNodes += (u32)mesh->getBvhNodes().size();

			_num_static_vertices += (u32)mesh->getVertices().size();
			_num_static_triangles += (u32)mesh->getTriangles().size();
			_num_static_emmisive_triangles += (u32)mesh->getEmmisiveTriangles().size();
			_num_static_materials += (u32)mesh->getMaterials().size();
			_num_static_bvh_nodes += (u32)mesh->getBvhNodes().size();
		}
	}

	InitBuffers(numVertices, numTriangles, numEmmisiveTriangles, numMaterials,
		numBvhNodes, (u32)scene->get_meshes().size() * 2, (u32)scene->get_lights().size());
	


	// Collect all static geometry and upload it to the GPU
	for (auto& meshBvhPair : scene->get_meshes())
	{
		auto mesh = meshBvhPair.mesh;
		if (mesh->isDynamic())
			continue;

		// TODO: use memcpy instead of looping over vertices (faster?)
		u32 startVertex = (u32)_vertices_host.size();
		for (auto& vertex : mesh->getVertices())
		{
			_vertices_host.push_back(vertex);
		}

		u32 startMaterial = (u32)_materials_host.size();
		for (auto& material : mesh->getMaterials())
		{
			_materials_host.push_back(material);
		}

		u32 startTriangle = (u32)_triangles_host.size();
		for (auto& triangle : mesh->getTriangles())
		{
			_triangles_host.push_back(triangle);
			_triangles_host.back().indices += startVertex;
			_triangles_host.back().material_index += startMaterial;
		}

		for (u32 triangle : mesh->getEmmisiveTriangles())
		{
			_emmisive_triangles_host.push_back(triangle + startTriangle);
		}

		u32 startBvhNode = (u32)_sub_bvh_nodes_host.size();
		for (auto& bvhNode : mesh->getBvhNodes())
		{
			_sub_bvh_nodes_host.push_back(bvhNode);
			auto& newNode = _sub_bvh_nodes_host.back();
			if (newNode.triangleCount > 0)
				newNode.firstTriangleIndex += startTriangle;
			else
				newNode.leftChildIndex += startBvhNode;
		}
		meshBvhPair.bvh_offset = startBvhNode;
	}

	writeToBuffer(_queue, _vertices[0], _vertices_host);
	writeToBuffer(_queue, _triangles[0], _triangles_host);
	writeToBuffer(_queue, _emmisive_trangles[0], _emmisive_triangles_host);
	writeToBuffer(_queue, _materials[0], _materials_host);
	writeToBuffer(_queue, _sub_bvh[0], _sub_bvh_nodes_host);

	writeToBuffer(_queue, _vertices[1], _vertices_host);
	writeToBuffer(_queue, _triangles[1], _triangles_host);
	writeToBuffer(_queue, _emmisive_trangles[1], _emmisive_triangles_host);
	writeToBuffer(_queue, _materials[1], _materials_host);
	writeToBuffer(_queue, _sub_bvh[1], _sub_bvh_nodes_host);

	writeToBuffer(_queue, _lights, scene->get_lights());

	// Copy textures to the GPU
	for (uint texId = 0; texId < Texture::getNumUniqueSurfaces(); texId++)
	{
		Tmpl8::Surface* surface = Texture::getSurface(texId);

		// Origin (o) and region (r)
		cl::size_t<3> o; o[0] = 0; o[1] = 0; o[2] = texId;

		cl::size_t<3> r;
		r[0] = Texture::TEXTURE_WIDTH;
		r[1] = Texture::TEXTURE_HEIGHT;
		r[2] = 1;// r[2] must be 1?
		cl_int err = _queue.enqueueWriteImage(
			_material_textures,
			CL_TRUE,
			o,
			r,
			0,
			0,
			surface->GetBuffer());
		checkClErr(err, "CommandQueue::enqueueWriteImage");
	}
}

void raytracer::RayTracer::SetTarget(GLuint glTexture)
{
	cl_int err;
	_output_image = cl::ImageGL(_context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, glTexture, &err);
	checkClErr(err, "ImageGL");
}

void raytracer::RayTracer::RayTrace(Camera& camera)
{
	// We must make sure that OpenGL is done with the textures, so we ask to sync.
	glFinish();
	
	std::vector<cl::Memory> images = { _output_image };
	_queue.enqueueAcquireGLObjects(&images);

	if (camera.dirty)
	{
		ClearAccumulationBuffer();
		_rays_per_pixel = 0;
		camera.dirty = false;
	}

	if (_rays_per_pixel >= MAX_RAYS_PER_PIXEL)
		return;

	// Non blocking CPU
	TraceRays(camera);
	Accumulate();
	GammaCorrection();

	// Lot of CPU work
	CopyNextFramesData();

	// Before returning the objects to OpenGL, we sync to make sure OpenCL is done.
	cl_int err = _queue.finish();
	checkClErr(err, "CommandQueue::finish");

	_queue.enqueueReleaseGLObjects(&images);

	_active_buffers = (_active_buffers + 1) % 2;
}

void raytracer::RayTracer::TraceRays(const Camera& camera)
{
	glm::vec3 eye;
	glm::vec3 scr_base_origin;
	glm::vec3 scr_base_u;
	glm::vec3 scr_base_v;
	camera.get_frustum(eye, scr_base_origin, scr_base_u, scr_base_v);

	glm::vec3 u_step = scr_base_u / (float)_scr_width;
	glm::vec3 v_step = scr_base_v / (float)_scr_height;

	

	// Copy camera (and scene) data to the device using a struct so we dont use 20 kernel arguments
	KernelData data = {};
	data.eye = glmToCl(eye);
	data.screen = glmToCl(scr_base_origin);
	data.u_step = glmToCl(u_step);
	data.v_step = glmToCl(v_step);
	data.width = _scr_width;

	data.numVertices = _num_static_vertices;
	data.numTriangles = _num_static_triangles;
	data.numEmmisiveTriangles = _num_static_emmisive_triangles;
	data.numLights = _num_lights;
	data.topLevelBvhRoot = _top_bvh_root_node[_active_buffers];

	data.raysPerPass = RAYS_PER_PASS;

	cl_int err = _queue.enqueueWriteBuffer(
		_ray_kernel_data,
		CL_TRUE,
		0,
		sizeof(KernelData),
		&data);
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");

	
	cl::Event kernelEvent;
	_ray_trace_kernel.setArg(0, _accumulation_buffer);
	_ray_trace_kernel.setArg(1, _ray_kernel_data);
	_ray_trace_kernel.setArg(2, _vertices[_active_buffers]);
	_ray_trace_kernel.setArg(3, _triangles[_active_buffers]);
	_ray_trace_kernel.setArg(4, _emmisive_trangles[_active_buffers]);
	_ray_trace_kernel.setArg(5, _materials[_active_buffers]);
	_ray_trace_kernel.setArg(6, _material_textures);
	_ray_trace_kernel.setArg(7, _lights);
	_ray_trace_kernel.setArg(8, _sub_bvh[_active_buffers]);
	_ray_trace_kernel.setArg(9, _top_bvh[_active_buffers]);
	_ray_trace_kernel.setArg(10, _random_streams);

	err = _queue.enqueueNDRangeKernel(
		_ray_trace_kernel,
		cl::NullRange,
		cl::NDRange(_scr_width, _scr_height),
		cl::NullRange,
		NULL,
		&kernelEvent);
	checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");

	_rays_per_pixel += RAYS_PER_PASS;
}

void raytracer::RayTracer::Accumulate()
{
	_accumulate_kernel.setArg(0, _accumulation_buffer);
	_accumulate_kernel.setArg(1, _output_image);
	_accumulate_kernel.setArg(2, _rays_per_pixel);
	_accumulate_kernel.setArg(3, _scr_width);
	_queue.enqueueNDRangeKernel(
		_accumulate_kernel,
		cl::NullRange,
		cl::NDRange(_scr_width, _scr_height),
		cl::NullRange,
		nullptr,
		nullptr);
}

void raytracer::RayTracer::GammaCorrection()
{
}

void raytracer::RayTracer::ClearAccumulationBuffer()
{
	cl_float3 zero; zero.x = 0.0f, zero.y = 0.0f, zero.z = 0.0f, zero.w = 0.0f;
	_queue.enqueueFillBuffer(
		_accumulation_buffer,
		zero,
		0,
		_scr_width * _scr_height * sizeof(cl_float3),
		nullptr,
		nullptr);
}

void raytracer::RayTracer::CopyNextFramesData()
{
	// Manually flush the queue
	// At least on AMD, the queue is flushed after the enqueueWriteBuffer (probably because it thinks
	//  one kernel launch is not enough reason to flush). So it would be executed at _queue.finish(), which
	//  is called after the top lvl bvh construction (which is expensive) has completed. Instead we manually
	//  flush and than calculate the top lvl bvh.
	_queue.flush();

	int copyBuffers = (_active_buffers + 1) % 2;
	std::vector<cl::Event> waitEvents;

	_vertices_host.resize(_num_static_vertices);
	_triangles_host.resize(_num_static_triangles);
	_materials_host.resize(_num_static_materials);
	_sub_bvh_nodes_host.resize(_num_static_bvh_nodes);

	// Collect all static geometry and upload it to the GPU
	for (auto& meshBvhPair : _scene->get_meshes())
	{
		auto mesh = meshBvhPair.mesh;
		if (!mesh->isDynamic())
			continue;

		mesh->buildBvh();

		// TODO: use memcpy instead of looping over vertices (faster?)
		u32 startVertex = (u32)_vertices_host.size();
		for (auto& vertex : mesh->getVertices())
		{
			_vertices_host.push_back(vertex);
		}

		u32 startMaterial = (u32)_materials_host.size();
		for (auto& material : mesh->getMaterials())
		{
			_materials_host.push_back(material);
		}

		u32 startTriangle = (u32)_triangles_host.size();
		for (auto& triangle : mesh->getTriangles())
		{
			_triangles_host.push_back(triangle);
			_triangles_host.back().indices += startVertex;
			_triangles_host.back().material_index += startMaterial;
		}

		u32 startBvhNode = (u32)_sub_bvh_nodes_host.size();
		for (auto& bvhNode : mesh->getBvhNodes())
		{
			_sub_bvh_nodes_host.push_back(bvhNode);
			auto& newNode = _sub_bvh_nodes_host.back();
			if (newNode.triangleCount > 0)
				newNode.firstTriangleIndex += startTriangle;
			else
				newNode.leftChildIndex += startBvhNode;
		}
		meshBvhPair.bvh_offset = startBvhNode;
	}

	if (_vertices_host.size() > static_cast<size_t>(_num_static_vertices))// Dont copy if we dont have any dynamic geometry
	{
		writeToBuffer(_copyQueue, _vertices[copyBuffers], _vertices_host, _num_static_vertices, waitEvents);
		writeToBuffer(_copyQueue, _triangles[copyBuffers], _triangles_host, _num_static_triangles, waitEvents);
		writeToBuffer(_copyQueue, _materials[copyBuffers], _materials_host, _num_static_materials, waitEvents);
		writeToBuffer(_copyQueue, _sub_bvh[copyBuffers], _sub_bvh_nodes_host, _num_static_bvh_nodes, waitEvents);
	}


	// Update the top level BVH and copy it to the GPU on a seperate copy queue
	_top_bvh_nodes_host.clear();
	auto bvhBuilder = TopLevelBvhBuilder(*_scene.get());
	_top_bvh_root_node[copyBuffers] = bvhBuilder.build(_sub_bvh_nodes_host, _top_bvh_nodes_host);

	writeToBuffer(_copyQueue, _top_bvh[copyBuffers], _top_bvh_nodes_host, 0, waitEvents);

	if (_vertices_host.size() > static_cast<size_t>(_num_static_vertices))
	{
		timeOpenCL(waitEvents[0], "vertex upload");
		timeOpenCL(waitEvents[1], "triangle upload");
		timeOpenCL(waitEvents[2], "material upload");
		timeOpenCL(waitEvents[3], "sub bvh upload");
		timeOpenCL(waitEvents[4], "top bvh upload");
	}
	else {
		timeOpenCL(waitEvents[0], "top bvh upload");
	}

	// Make sure the main queue waits for the copy to finish
	cl_int err = _queue.enqueueBarrierWithWaitList(&waitEvents);
	checkClErr(err, "CommandQueue::enqueueBarrierWithWaitList");
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
	clGetPlatformIDs(0, nullptr, &lNbPlatformId);

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
#ifdef PROFILE_OPENCL
	cl_command_queue_properties props = CL_QUEUE_PROFILING_ENABLE;
#else
	cl_command_queue_properties props = 0;
#endif
	_queue = clCreateCommandQueue(lContext, lDeviceId, props, &lError);
	checkClErr(lError, "Unable to create an OpenCL command queue.");
	_copyQueue = clCreateCommandQueue(lContext, lDeviceId, props, &lError);
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
	err = program.build(_devices, "-I assets/cl/ -I assets/cl/clRNG/");
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
