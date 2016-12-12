#pragma once
//#include "template/template.h"// Includes template/cl.hpp
#include "template/includes.h"
#include "bvh.h"
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

	void SetScene(Scene& scene);
	void SetTarget(GLuint glTexture);
	void RayTrace(const Camera& camera);
private:
	void InitOpenCL();
	void InitBuffers();

	void UpdateTopLevelBVH();
	cl::Kernel LoadKernel(const char* fileName, const char* funcName);
private:
	cl_uint _scr_width, _scr_height;

	std::unique_ptr<Bvh> _bvh;

	cl::Context _context;
	std::vector<cl::Device> _devices;
	cl::CommandQueue _queue;

	cl::Kernel _helloWorldKernel;

	cl::Buffer _kernel_data;

	cl_int _num_vertices;
	cl::Buffer _vertices;
	cl_int _num_triangles;
	cl::Buffer _triangles;

	cl_int _num_mesh_materials;
	size_t _num_textures;
	cl::Buffer _materials;
	
	cl_int _num_lights;
	cl::Buffer _lights;

	cl_int _num_fat_bvh_nodes;
	cl::Buffer _fat_bvh;

	cl_int _num_thin_bvh_nodes;
	cl::Buffer _thin_bvh;

	cl::Image2DArray _material_textures;
	cl::ImageGL _output_image;
};
}

