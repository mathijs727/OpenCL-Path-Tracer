#pragma once
//#include "template/template.h"// Includes template/cl.hpp
#include "template/includes.h"
#include "top_bvh.h"
#include "vertices.h"
#include "material.h"
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

	void SetScene(std::shared_ptr<Scene> scene);
	void SetTarget(GLuint glTexture);
	void RayTrace(const Camera& camera);
private:
	void InitOpenCL();
	void InitBuffers(
		u32 numVertices,
		u32 numTriangles,
		u32 numMaterials,
		u32 numSubBvhNodes,
		u32 numTopBvhNodes,
		u32 numLights);

	cl::Kernel LoadKernel(const char* fileName, const char* funcName);
private:
	std::shared_ptr<Scene> _scene;

	cl_uint _scr_width, _scr_height;

	cl::Context _context;
	std::vector<cl::Device> _devices;
	cl::CommandQueue _queue;
	cl::CommandQueue _copyQueue;

	cl::Kernel _helloWorldKernel;
	cl::Buffer _kernel_data;

	std::vector<VertexSceneData> _vertices_host;
	std::vector<TriangleSceneData> _triangles_host;
	std::vector<Material> _materials_host;
	std::vector<SubBvhNode> _sub_bvh_nodes_host;

	cl_int _num_static_vertices;
	cl_int _num_static_triangles;
	cl_int _num_static_materials;
	cl_int _num_static_bvh_nodes;
	uint _active_buffers = 0;
	cl::Buffer _vertices[2];
	cl::Buffer _triangles[2];
	cl::Buffer _materials[2];
	cl::Buffer _sub_bvh[2];

	cl_int _num_lights;
	cl::Buffer _lights;
	
	cl::Image2DArray _material_textures;

	std::vector<TopBvhNode> _top_bvh_nodes_host;
	cl_int _top_bvh_root_node[2];
	cl::Buffer _top_bvh[2];

	cl::ImageGL _output_image;
};




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

}

