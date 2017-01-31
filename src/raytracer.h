#pragma once
//#include "template/template.h"// Includes template/cl.hpp
#include "template/includes.h"
#include "top_bvh.h"
#include "vertices.h"
#include "material.h"
#include <memory>
#include <vector>
#include "hdrtexture.h"

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

	void SetSkydome(const char* filePathFormat, bool isLinear, float multiplier);

	void SetScene(std::shared_ptr<Scene> scene);
	void SetTarget(GLuint glTexture);
	void RayTrace(Camera& camera);

	void FrameTick();// Load next animation frame data

	int GetNumPasses();
	int GetMaxPasses();
private:
	void TraceRays(const Camera& camera);

	void Accumulate(const Camera& camera);
	void ClearAccumulationBuffer();
	void CalculateAverageGrayscale();

	void CopyNextAnimationFrameData();
	void CollectTransformedLights(const SceneNode* node, const glm::mat4& transform);

	void InitOpenCL();
	void InitBuffers(
		u32 numVertices,
		u32 numTriangles,
		u32 numEmissiveTriangles,
		u32 numMaterials,
		u32 numSubBvhNodes,
		u32 numTopBvhNodes,
		u32 numLights);

	cl::Kernel LoadKernel(const char* fileName, const char* funcName);
private:
	std::shared_ptr<Scene> _scene;

	std::unique_ptr<HDRTexture> _skydome;
	bool _skydome_loaded;

	cl_uint _scr_width, _scr_height;

	cl::Context _context;
	cl::Device _device;
	cl::CommandQueue _queue;
	cl::CommandQueue _copyQueue;

	cl::Kernel _generate_rays_kernel;
	cl::Kernel _intersect_walk_kernel;
	cl::Kernel _shading_kernel;
	cl::Kernel _intersect_shadows_kernel;
	cl::Kernel _update_kernel_data_kernel;

	cl::Buffer _ray_traversal_buffer;
	cl::Buffer _ray_kernel_data;
	cl::Buffer _random_streams;
	cl::Buffer _rays_buffers[2];
	cl::Buffer _shading_buffer;
	cl::Buffer _shadow_rays_buffer;

	cl_uint _rays_per_pixel;
	cl::Kernel _accumulate_kernel;
	cl::Buffer _accumulation_buffer;
	cl::ImageGL _output_image;

	GLuint _output_image_gl;
	cl::Image2D _output_image_cl;
	std::unique_ptr<float[]> _output_image_cpu;

	std::vector<VertexSceneData> _vertices_host;
	std::vector<TriangleSceneData> _triangles_host;
	std::vector<EmissiveTriangle> _emissive_triangles_host;
	std::vector<Material> _materials_host;
	std::vector<SubBvhNode> _sub_bvh_nodes_host;

	cl_uint _num_static_vertices;
	cl_uint _num_static_triangles;
	cl_uint _num_emissive_triangles[2];
	cl_uint _num_static_materials;
	cl_uint _num_static_bvh_nodes;
	uint _active_buffers = 0;
	cl::Buffer _vertices[2];
	cl::Buffer _triangles[2];
	cl::Buffer _emissive_trangles[2];
	cl::Buffer _materials[2];
	cl::Buffer _sub_bvh[2];
	
	cl::Image2D _no_texture;
	cl::Image2D _skydome_texture;
	cl::Image2DArray _material_textures;

	std::vector<TopBvhNode> _top_bvh_nodes_host;
	cl_uint _top_bvh_root_node[2];
	cl::Buffer _top_bvh[2];
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

