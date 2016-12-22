#pragma once
//#include "template/template.h"// Includes template/cl.hpp
#include "template/includes.h"
#include "top_bvh.h"
#include <memory>
#include <vector>

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

	cl::Kernel LoadKernel(const char* fileName, const char* funcName);
private:
	std::unique_ptr<TopLevelBvhBuilder> _bvhBuilder;

	cl_uint _scr_width, _scr_height;

	cl::Context _context;
	std::vector<cl::Device> _devices;
	cl::CommandQueue _queue;
	cl::CommandQueue _copyQueue;

	cl::Kernel _helloWorldKernel;

	cl::Buffer _kernel_data;

	cl_int _num_vertices;
	cl::Buffer _vertices;
	cl_int _num_triangles;
	cl::Buffer _triangles;

	cl_int _num_mesh_materials;
	cl_int _num_textures;
	cl::Buffer _materials;
	
	cl_int _num_lights;
	cl::Buffer _lights;

	uint _active_top_bvh = 0;
	cl_int _num_top_bvh_nodes[2];
	cl::Buffer _top_bvh[2];

	cl_int _num_sub_bvh_nodes;
	cl::Buffer _sub_bvh;

	cl::Image2DArray _material_textures;
	cl::ImageGL _output_image;
};
}

