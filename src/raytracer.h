#pragma once
//#include "template/template.h"// Includes template/cl.hpp
#include "template/includes.h"
#include <memory>

namespace Tmpl8 {
class Surface;
}

namespace raytracer {

class Camera;
class Scene;


class RayTracer
{
public:
	RayTracer(int width, int height);
	~RayTracer();

	void SetScene(const Scene& scene);
	void SetTarget(GLuint glTexture);
	void RayTrace(const Camera& camera);
private:
	void InitOpenCL();
	void InitBuffers();
	cl::Kernel LoadKernel(const char* fileName, const char* funcName);
private:
	size_t _buffer_size;
	cl_uint _scr_width, _scr_height;

	cl::Context _context;
	std::vector<cl::Device> _devices;
	cl::CommandQueue _queue;

	cl::Kernel _helloWorldKernel;

	std::unique_ptr<float[]> _outHost;
	cl::Buffer _outDevice;

	cl_int _num_spheres;
	cl::Buffer _spheres;
	cl_int _num_planes;
	cl::Buffer _planes;
	cl::Buffer _materials;

	cl_int _num_mesh_materials;
	cl_int _num_vertices;
	cl::Buffer _vertices;
	cl_int _num_triangles;
	cl::Buffer _triangles;
	cl_int _num_lights;
	cl::Buffer _lights;

	cl::ImageGL _outputImage;
};
}

