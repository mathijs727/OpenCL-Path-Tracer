#pragma once
#include "template\cl.hpp"
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

	void RayTrace(const Camera& camera, const Scene& scene, Tmpl8::Surface& target_surface);
private:
	void InitOpenCL();
	void InitBuffers();
	cl::Kernel LoadKernel(const char* fileName, const char* funcName);
private:
	size_t _buffer_size;
	int _scr_width, _scr_height;

	cl::Context _context;
	std::vector<cl::Device> _devices;
	cl::CommandQueue _queue;

	cl::Kernel _helloWorldKernel;

	std::unique_ptr<float[]> _outHost;
	cl::Buffer _outDevice;
	cl::Buffer _spheres;
};

void raytrace(const Camera& camera, const Scene& scene, Tmpl8::Surface& target_surface);
}

