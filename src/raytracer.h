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
	RayTracer();
	~RayTracer();

	void DoSomething();
private:
	void InitOpenCL();
	void InitBuffers();
	cl::Kernel LoadKernel(const char* fileName, const char* funcName);
private:
	cl::Context _context;
	std::vector<cl::Device> _devices;
	cl::CommandQueue _queue;

	cl::Kernel _helloWorldKernel;

	std::unique_ptr<char[]> _outH;
	cl::Buffer _outCL;
};

void raytrace(const Camera& camera, const Scene& scene, Tmpl8::Surface& target_surface);
}

