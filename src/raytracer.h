#pragma once
#include "template\cl.hpp"
#include <memory>
#include <OpenGL\glew.h>

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
	int _scr_width, _scr_height;

	cl::Context _context;
	std::vector<cl::Device> _devices;
	cl::CommandQueue _queue;

	cl::Kernel _helloWorldKernel;

	std::unique_ptr<float[]> _outHost;
	cl::Buffer _outDevice;

	int _num_spheres;
	cl::Buffer _spheres;
	int _num_planes;
	cl::Buffer _planes;
	cl::Buffer _materials;
	int _num_lights;
	cl::Buffer _lights;

	cl::ImageGL _outputImage;
};

void raytrace(const Camera& camera, const Scene& scene, Tmpl8::Surface& target_surface);
}

