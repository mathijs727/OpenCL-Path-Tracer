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

	void SetScene(Scene& scene);
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
	
	template<typename T>
	void blockingWrite(cl::Buffer& buffer, std::vector<T> items);
private:
	std::unique_ptr<TopLevelBvhBuilder> _bvhBuilder;

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
	cl_int _num_lights;
	cl::Buffer _vertices;
	cl::Buffer _triangles;
	cl::Buffer _materials;
	cl::Buffer _sub_bvh;
	cl::Buffer _lights;
	
	cl::Image2DArray _material_textures;

	uint _active_top_bvh = 0;
	cl_int _num_top_bvh_nodes[2];
	std::vector<TopBvhNode> _top_bvh_host;
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

template<typename T>
inline void RayTracer::blockingWrite(cl::Buffer& buffer, std::vector<T> items)
{
	if (items.size() == 0)
		return;

	cl_int err = _queue.enqueueWriteBuffer(
		buffer,
		CL_TRUE,
		0,
		items.size() * sizeof(T),
		items.data());
	checkClErr(err, "CommandQueue::enqueueWriteBuffer");
}

}

