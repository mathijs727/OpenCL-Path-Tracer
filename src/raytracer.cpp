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
#include <clRNG\lfsr113.h>

//#define PROFILE_OPENCL
#define OPENCL_GL_INTEROP
//#define OUTPUT_AVERAGE_GRAYSCALE
#define MAX_RAYS_PER_PIXEL 5000
#define MAX_NUM_LIGHTS 256

#define MAX_ACTIVE_RAYS 1280*720// Number of rays per pass (top performance = all pixels in 1 pass but very large buffer sizes at 4K?)

struct KernelData
{
	raytracer::CameraData camera;

	// Scene
	uint numEmissiveTriangles;
	uint topLevelBvhRoot;

	// Used for ray generation
	uint rayOffset;
	uint scrWidth;
	uint scrHeight;

	// Used for ray compaction
	uint numInRays;
	uint numOutRays;
	uint numShadowRays;
	uint maxRays;
	uint newRays;
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



inline static size_t toMultipleOf(size_t N, size_t base)
{
	return static_cast<size_t>((ceil((double)N / (double)base) * base));
}

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

// http://stackoverflow.com/questions/3407012/c-rounding-up-to-the-nearest-multiple-of-a-number
inline int roundUp(int numToRound, int multiple)
{
	if (multiple == 0)
		return numToRound;

	int remainder = numToRound % multiple;
	if (remainder == 0)
		return numToRound;

	return numToRound + multiple - remainder;
}




raytracer::RayTracer::RayTracer(int width, int height) : _rays_per_pixel(0)
{
	_scr_width = width;
	_scr_height = height;

	InitOpenCL();

	_generate_rays_kernel = LoadKernel("assets/cl/kernel.cl", "generatePrimaryRays");
	_intersect_shadows_kernel = LoadKernel("assets/cl/kernel.cl", "intersectShadows");
	_intersect_walk_kernel = LoadKernel("assets/cl/kernel.cl", "intersectWalk");
	_shading_kernel = LoadKernel("assets/cl/kernel.cl", "shade");
	_update_kernel_data_kernel = LoadKernel("assets/cl/kernel.cl", "updateKernelData");
	_accumulate_kernel = LoadKernel("assets/cl/accumulate.cl", "accumulate");

	_top_bvh_root_node[0] = 0;
	_top_bvh_root_node[1] = 0;

	_num_emissive_triangles[0] = 0;
	_num_emissive_triangles[1] = 0;
}

raytracer::RayTracer::~RayTracer()
{
}

void raytracer::RayTracer::SetScene(std::shared_ptr<Scene> scene)
{
	_scene = scene;

	// Initialize buffers
	_num_static_vertices = 0;
	_num_static_triangles = 0;
	_num_static_materials = 0;
	_num_static_bvh_nodes = 0;

	u32 numVertices = 0;
	u32 numTriangles= 0;
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
			numMaterials += (u32)mesh->getMaterials().size();
			numBvhNodes += (u32)mesh->getBvhNodes().size();

			_num_static_vertices += (u32)mesh->getVertices().size();
			_num_static_triangles += (u32)mesh->getTriangles().size();
			_num_static_materials += (u32)mesh->getMaterials().size();
			_num_static_bvh_nodes += (u32)mesh->getBvhNodes().size();
		}
	}

	InitBuffers(numVertices, numTriangles, MAX_NUM_LIGHTS, numMaterials,
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
	writeToBuffer(_queue, _materials[0], _materials_host);
	writeToBuffer(_queue, _sub_bvh[0], _sub_bvh_nodes_host);

	writeToBuffer(_queue, _vertices[1], _vertices_host);
	writeToBuffer(_queue, _triangles[1], _triangles_host);
	writeToBuffer(_queue, _materials[1], _materials_host);
	writeToBuffer(_queue, _sub_bvh[1], _sub_bvh_nodes_host);

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

	FrameTick();
}

void raytracer::RayTracer::SetTarget(GLuint glTexture)
{
	cl_int err;
#ifndef OPENCL_GL_INTEROP
	_output_image_cl = cl::Image2D(_context,
		CL_MEM_WRITE_ONLY,
		cl::ImageFormat(CL_RGBA, CL_SNORM_INT8),
		_scr_width,
		_scr_height,
		0,
		0,
		&err);
	checkClErr(err, "Image2D");

	_output_image_gl = glTexture;
	size_t size = _scr_width * _scr_height * 4;
	std::cout << "Output image cpu size: " << size << std::endl;
	float* mem = new float[size];
	_output_image_cpu = std::unique_ptr<float[]>(mem);// std::make_unique<float[]>(size);
#else
	_output_image = cl::ImageGL(_context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, glTexture, &err);
	checkClErr(err, "ImageGL");
#endif
}

void raytracer::RayTracer::RayTrace(Camera& camera)
{
#ifdef OPENCL_GL_INTEROP
	// We must make sure that OpenGL is done with the textures, so we ask to sync.
	glFinish();
	
	std::vector<cl::Memory> images = { _output_image };
	_queue.enqueueAcquireGLObjects(&images);
#endif

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
#ifdef OUTPUT_AVERAGE_GRAYSCALE
	CalculateAverageGrayscale();
#endif
	GammaCorrection();
	
	_queue.finish();

#ifndef OPENCL_GL_INTEROP
	// Copy OpenCL image to the CPU
	cl::size_t<3> o; o[0] = 0; o[1] = 0; o[2] = 0;
	cl::size_t<3> r; r[0] = _scr_width; r[1] = _scr_height; r[2] = 1;
	_queue.enqueueReadImage(_output_image_cl, CL_TRUE, o, r, 0, 0, _output_image_cpu.get(), nullptr, nullptr);

	// And upload it to teh GPU (OpenGL)
	glBindTexture(GL_TEXTURE_2D, _output_image_gl);
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		_scr_width,
		_scr_height,
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		_output_image_cpu.get());
#else
	// Before returning the objects to OpenGL, we sync to make sure OpenCL is done.
	cl_int err = _queue.finish();
	checkClErr(err, "CommandQueue::finish");

	_queue.enqueueReleaseGLObjects(&images);
#endif
}

void raytracer::RayTracer::FrameTick()
{
	// Lot of CPU work
	CopyNextAnimationFrameData();

	_active_buffers = (_active_buffers + 1) % 2;
}

void raytracer::RayTracer::TraceRays(const Camera& camera)
{
	// Copy camera (and scene) data to the device using a struct so we dont use 20 kernel arguments
	KernelData data = {};

	data.camera = camera.get_camera_data();

	data.numEmissiveTriangles = _num_emissive_triangles[_active_buffers];
	data.topLevelBvhRoot = _top_bvh_root_node[_active_buffers];

	data.rayOffset = 0;
	data.scrWidth = (u32)_scr_width;
	data.scrHeight = (u32)_scr_height;

	data.numInRays = 0;
	data.numOutRays = 0;
	data.numShadowRays = 0;
	data.maxRays = MAX_ACTIVE_RAYS;
	data.newRays = 0;

	cl_int err = _queue.enqueueWriteBuffer(
		_ray_kernel_data,
		CL_TRUE,
		0,
		sizeof(KernelData),
		&data);
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");

	static_assert(MAX_ACTIVE_RAYS, "MAX_ACTIVE_RAYS must be a multiple of 64 (work group size)");
	int inRayBuffer = 0;
	int outRayBuffer = 1;
	u32 survivingRays = 0;
	while (true)
	{
		if (survivingRays != MAX_ACTIVE_RAYS)
		{
			// Generate primary rays and fill the emptyness
			_generate_rays_kernel.setArg(0, _rays_buffers[inRayBuffer]);
			_generate_rays_kernel.setArg(1, _ray_kernel_data);
			_generate_rays_kernel.setArg(2, _random_streams);
			err = _queue.enqueueNDRangeKernel(
				_generate_rays_kernel,
				cl::NullRange,
				cl::NDRange(roundUp(MAX_ACTIVE_RAYS - survivingRays, 32)),
				cl::NullRange);//cl::NDRange(64));
			checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");
		}



		// Output data
		_intersect_walk_kernel.setArg(0, _shading_buffer);
		// Input data
		_intersect_walk_kernel.setArg(1, _rays_buffers[inRayBuffer]);
		_intersect_walk_kernel.setArg(2, _ray_kernel_data);
		_intersect_walk_kernel.setArg(3, _vertices[_active_buffers]);
		_intersect_walk_kernel.setArg(4, _triangles[_active_buffers]);
		_intersect_walk_kernel.setArg(5, _sub_bvh[_active_buffers]);
		_intersect_walk_kernel.setArg(6, _top_bvh[_active_buffers]);
		_intersect_walk_kernel.setArg(7, _accumulation_buffer);

		err = _queue.enqueueNDRangeKernel(
			_intersect_walk_kernel,
			cl::NullRange,
			cl::NDRange(MAX_ACTIVE_RAYS),
			cl::NDRange(64));
		checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");



		// Output data
		_shading_kernel.setArg(0, _accumulation_buffer);
		_shading_kernel.setArg(1, _rays_buffers[outRayBuffer]);
		_shading_kernel.setArg(2, _shadow_rays_buffer);
		// Input data
		_shading_kernel.setArg(3, _rays_buffers[inRayBuffer]);
		_shading_kernel.setArg(4, _shading_buffer);
		_shading_kernel.setArg(5, _ray_kernel_data);
		// Static input data
		_shading_kernel.setArg(6, _vertices[_active_buffers]);
		_shading_kernel.setArg(7, _triangles[_active_buffers]);
		_shading_kernel.setArg(8, _emissive_trangles[_active_buffers]);
		_shading_kernel.setArg(9, _materials[_active_buffers]);
		_shading_kernel.setArg(10, _material_textures);
		_shading_kernel.setArg(11, _random_streams);

		err = _queue.enqueueNDRangeKernel(
			_shading_kernel,
			cl::NullRange,
			cl::NDRange(MAX_ACTIVE_RAYS),
			cl::NDRange(64));
		checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");



		// Request the output kernel data so we know the amount of surviving rays
		cl::Event updatedKernelDataEvent;
		KernelData updatedKernelData;
		err = _queue.enqueueReadBuffer(
			_ray_kernel_data,
			CL_TRUE,
			0,
			sizeof(KernelData),
			&updatedKernelData);
		survivingRays = updatedKernelData.numOutRays;
		// Stop if we reach 0 out rays and we processed the whole screen
		//updatedKernelDataEvent.wait();
		uint maxRays = _scr_width * _scr_height;
		if (survivingRays == 0 &&// We are out of rays
			(updatedKernelData.rayOffset + updatedKernelData.newRays >= maxRays))// And we wont generate new ones
			break;
		//survivingRays = MAX_ACTIVE_RAYS;


		if (survivingRays != 0)
		{
			_intersect_shadows_kernel.setArg(0, _accumulation_buffer);
			_intersect_shadows_kernel.setArg(1, _shadow_rays_buffer);
			_intersect_shadows_kernel.setArg(2, _ray_kernel_data);
			_intersect_shadows_kernel.setArg(3, _vertices[_active_buffers]);
			_intersect_shadows_kernel.setArg(4, _triangles[_active_buffers]);
			_intersect_shadows_kernel.setArg(5, _emissive_trangles[_active_buffers]);
			_intersect_shadows_kernel.setArg(6, _materials[_active_buffers]);
			_intersect_shadows_kernel.setArg(7, _sub_bvh[_active_buffers]);
			_intersect_shadows_kernel.setArg(8, _top_bvh[_active_buffers]);

			err = _queue.enqueueNDRangeKernel(
				_intersect_shadows_kernel,
				cl::NullRange,
				cl::NDRange(roundUp(survivingRays, 64)),
				cl::NDRange(64));
			checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");
		}


		// Set num input rays to num output rays and set num out rays and num shadow rays to 0
		_update_kernel_data_kernel.setArg(0, _ray_kernel_data);
		err = _queue.enqueueNDRangeKernel(
			_update_kernel_data_kernel,
			cl::NullRange,
			cl::NDRange(1),
			cl::NDRange(1));
		checkClErr(err, "CommandQueue::enqueueNDRangeKernel()");



		// What used to be output is now the input to the pass
		std::swap(inRayBuffer, outRayBuffer);
	}

	_rays_per_pixel += 1;
}

void raytracer::RayTracer::Accumulate()
{
	_accumulate_kernel.setArg(0, _accumulation_buffer);
#ifdef OPENCL_GL_INTEROP
	_accumulate_kernel.setArg(1, _output_image);
#else
	_accumulate_kernel.setArg(1, _output_image_cl);
#endif
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

void raytracer::RayTracer::CalculateAverageGrayscale()
{
	size_t sizeInVecs = _scr_width * _scr_height;
	auto buffer =  std::make_unique<glm::vec4[]>(sizeInVecs);
	_queue.enqueueReadBuffer(
		_accumulation_buffer,
		CL_TRUE,
		0,
		sizeInVecs * sizeof(cl_float3),
		buffer.get(),
		nullptr,
		nullptr);

	float sumLeft = 0.0f;
	float sumRight = 0.0f;
	for (size_t i = 0; i < sizeInVecs; i++)
	{
		// https://en.wikipedia.org/wiki/Grayscale
		glm::vec3 colour = glm::vec3(buffer[i]) / (float)_rays_per_pixel;
		float grayscale = 0.2126f * colour.r + 0.7152f * colour.g + 0.0722f * colour.b;

		auto col = i % _scr_width;
		if (col < _scr_width / 2)
			sumLeft += grayscale;
		else
			sumRight += grayscale;
	}
	sumLeft /= (float)sizeInVecs * 2;
	sumRight /= (float)sizeInVecs * 2;
	std::cout << "Average grayscale value left:  " << sumLeft << std::endl;
	std::cout << "Average grayscale value right: " << sumRight << "\n" << std::endl;
}

void raytracer::RayTracer::CopyNextAnimationFrameData()
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

	// Get the light emmiting triangles transformed by the scene graph
	_emissive_triangles_host.clear();
	CollectTransformedLights(&_scene->get_root_node(), glm::mat4());
	_num_emissive_triangles[copyBuffers] = (u32)_emissive_triangles_host.size();
	writeToBuffer(_copyQueue, _emissive_trangles[copyBuffers], _emissive_triangles_host, 0, waitEvents);

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
		timeOpenCL(waitEvents[4], "emissive triangles upload");
		timeOpenCL(waitEvents[5], "top bvh upload");
	}
	else {
		//timeOpenCL(waitEvents[0], "emissive triangles upload");
		//timeOpenCL(waitEvents[1], "top bvh upload");
	}

	// Make sure the main queue waits for the copy to finish
	cl_int err = _queue.enqueueBarrierWithWaitList(&waitEvents);
	checkClErr(err, "CommandQueue::enqueueBarrierWithWaitList");
}

void raytracer::RayTracer::CollectTransformedLights(const SceneNode* node, const glm::mat4& transform)
{
	auto& newTransform = transform * node->transform.matrix();
	if (node->mesh != -1)
	{
		auto& mesh = _scene->get_meshes()[node->mesh];
		auto& vertices = mesh.mesh->getVertices();
		auto& triangles = mesh.mesh->getTriangles();
		auto& emissiveTriangles = mesh.mesh->getEmissiveTriangles();
		auto& materials = mesh.mesh->getMaterials();

		for (auto& triangleIndex : emissiveTriangles)
		{
			auto& triangle = triangles[triangleIndex];
			EmissiveTriangle result;
			result.vertices[0] = newTransform * vertices[triangle.indices[0]].vertex;
			result.vertices[1] = newTransform * vertices[triangle.indices[1]].vertex;
			result.vertices[2] = newTransform * vertices[triangle.indices[2]].vertex;
			result.material = materials[triangle.material_index];
			_emissive_triangles_host.push_back(result);
		}
	}

	for (auto& child : node->children)
	{
		CollectTransformedLights(child.get(), newTransform);
	}
}





// http://developer.amd.com/tools-and-sdks/opencl-zone/opencl-resources/introductory-tutorial-to-opencl/
// https://www.codeproject.com/articles/685281/opengl-opencl-interoperability-a-case-study-using
void raytracer::RayTracer::InitOpenCL()
{
#ifdef OPENCL_GL_INTEROP
	setenv("CUDA_CACHE_DISABLE", "1", 1);
	cl_int lError = CL_SUCCESS;
	std::string lBuffer;

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
				&lError);
			if (lError == CL_SUCCESS)
			{
				// We found the context.
				lPlatformId = lPlatformIdToTry;
				lDeviceId = lDeviceIdToTry;
				lContext = lContextToTry;
				_device = cl::Device(lDeviceId);
				_context = cl::Context(lContext);
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
#else
	// Let the user select a platform
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	std::cout << "Platforms:" << std::endl;
	for (int i = 0; i < (int)platforms.size(); i++)
	{
		std::string platformName;
		platforms[i].getInfo(CL_PLATFORM_NAME, &platformName);
		std::cout << "[" << i << "] " << platformName << std::endl;
	}
	cl::Platform platform;
	{
		int platformIndex;
		std::cout << "Select a platform: ";
		std::cin >> platformIndex;
		//platformIndex = 0;
		platform = platforms[platformIndex];
	}

	// Let the user select a device
	std::vector<cl::Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
	std::cout << "\nDevices:" << std::endl;
	for (int i = 0; i < (int)devices.size(); i++)
	{
		std::string deviceName;
		devices[i].getInfo(CL_DEVICE_NAME, &deviceName);
		std::cout << "[" << i << "] " << deviceName << std::endl;
	}
	{
		int deviceIndex;
		std::cout << "Select a device: ";
		std::cin >> deviceIndex;
		//deviceIndex = 0;
		_device = devices[deviceIndex];
	}

	// Create OpenCL context
	cl_int lError;
	_context = cl::Context(devices, NULL, NULL, NULL, &lError);
	checkClErr(lError, "cl::Context");
#endif

	std::string openCLVersion;
	_device.getInfo(CL_DEVICE_VERSION, &openCLVersion);
	std::cout << "OpenCL version: " << openCLVersion << std::endl;

	// Create a command queue.
#ifdef PROFILE_OPENCL
	cl_command_queue_properties props = CL_QUEUE_PROFILING_ENABLE;
#else
	cl_command_queue_properties props = 0;
#endif
	checkClErr(lError, "Unable to create an OpenCL command queue.");
	_queue = cl::CommandQueue(_context, _device, props, &lError);
	checkClErr(lError, "Unable to create an OpenCL command queue.");
	_copyQueue = cl::CommandQueue(_context, _device, props, &lError);
	checkClErr(lError, "Unable to create an OpenCL command queue.");
}

void raytracer::RayTracer::InitBuffers(
	u32 numVertices,
	u32 numTriangles,
	u32 numEmissiveTriangles,
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

	_emissive_trangles[0] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numEmissiveTriangles) * sizeof(EmissiveTriangle),
		NULL,
		&err);
	_emissive_trangles[1] = cl::Buffer(_context,
		CL_MEM_READ_ONLY,
		std::max(1u, numEmissiveTriangles) * sizeof(EmissiveTriangle),
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
		CL_MEM_READ_WRITE,
		sizeof(KernelData),
		NULL,
		&err);
	checkClErr(err, "Buffer::Buffer()");

	// Create random streams and copy them to the GPU
	size_t numWorkItems = _scr_width * _scr_height;
	size_t streamBufferSize;
	clrngLfsr113Stream* streams = clrngLfsr113CreateStreams(
		NULL, numWorkItems, &streamBufferSize, (clrngStatus*)&err);
	checkClErr(err, "clrngLfsr113CreateStreams");
	_random_streams = cl::Buffer(_context,
		CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		streamBufferSize,
		streams,
		&err);
	checkClErr(err, "Buffer::Buffer()");
	clrngLfsr113DestroyStreams(streams);




	_accumulation_buffer = cl::Buffer(_context,
		CL_MEM_READ_WRITE,
		_scr_width * _scr_height * sizeof(cl_float3),
		nullptr,
		&err);
	checkClErr(err, "cl::Buffer");

	const int rayDataStructSize = 64;
	_rays_buffers[0] = cl::Buffer(_context,
		CL_MEM_READ_WRITE,
		(size_t)MAX_ACTIVE_RAYS * rayDataStructSize,
		nullptr,
		&err);
	checkClErr(err, "cl::Buffer");
	_rays_buffers[1] = cl::Buffer(_context,
		CL_MEM_READ_WRITE,
		(size_t)MAX_ACTIVE_RAYS * rayDataStructSize,
		nullptr,
		&err);
	checkClErr(err, "cl::Buffer");

	_shadow_rays_buffer = cl::Buffer(_context,
		CL_MEM_READ_WRITE,
		(size_t)MAX_ACTIVE_RAYS * rayDataStructSize,
		nullptr,
		&err);
	checkClErr(err, "cl::Buffer");

	const int shadingDataStructSize = 64;
	_shading_buffer = cl::Buffer(_context,
		CL_MEM_READ_WRITE,
		(size_t)MAX_ACTIVE_RAYS * shadingDataStructSize,
		nullptr,
		&err);
	checkClErr(err, "cl::Buffer");
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

	std::vector<cl::Device> devices;
	devices.push_back(_device);

	std::string prog(std::istreambuf_iterator<char>(file),
		(std::istreambuf_iterator<char>()));
	cl::Program::Sources sources;
	sources.push_back(std::make_pair(prog.c_str(), prog.length()));
	cl::Program program(_context, sources);
	std::string opts = "-I assets/cl/ -I assets/cl/clRNG/ ";
	/*opts += "-g -s \"D:/Documents/Development/UU/INFMAGR/infmagr-assignment1/";
	opts += fileName;
	opts += "\"";*/
	err = program.build(devices, opts.c_str());
	{
		if (err != CL_SUCCESS)
		{
			std::string errorMessage = "Cannot build program: ";
			errorMessage += fileName;
			std::cout << "Cannot build program: " << fileName << std::endl;

			std::string error;
			program.getBuildInfo(_device, CL_PROGRAM_BUILD_LOG, &error);
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
