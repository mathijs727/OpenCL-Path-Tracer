#pragma once
#include <vector>
#include "shapes.h"
#include "ray.h"
#include "material.h"
#include "light.h"
#include "mesh.h"

#define MAX_TRACE_DEPTH 6

namespace raytracer {

struct MeshSceneData
{
	u32 start_triangle_index;
	u32 triangle_count;
	u32 bvhIndex;
	bool is_empty() { return triangle_count == 0; }
	MeshSceneData(u32 startIndex = 0, u32 count = 0) : start_triangle_index(startIndex), triangle_count(count), bvhIndex(-1) {}
};

struct SceneNode
{
	SceneNode* parent = nullptr;
	std::vector<std::unique_ptr<SceneNode>> children;
	Transform transform;
	MeshSceneData meshData;
};

class Scene
{
public:
	friend class Bvh;

	Scene() { };

	SceneNode& add_node(const Mesh& primitive, const Transform& transform = Transform(), SceneNode* parent = nullptr);
	//SceneNode& add_node(const std::vector<Mesh>& primitive, const Transform& transform = Transform(), SceneNode* parent = nullptr);

	void add_light(Light& light)
	{
		_lights.push_back(light);
	}

	SceneNode& get_root_node() { return _root_node; }

	const std::vector<Light>& GetLights() const { return _lights; };
	//const std::vector<VertexSceneData>& GetVertices() const { return _vertices; }
	//std::vector<TriangleSceneData>& GetTriangleIndices() { return _triangle_indices; }
	//const std::vector<Material>& GetMeshMaterials() const { return _meshes_materials; }

public:
	struct LightIterableConst {
		const Scene& scene;

		LightIterableConst(const Scene& scene) : scene(scene) { }

		typedef std::vector<Light>::const_iterator const_iterator;

		const_iterator begin() { return scene._lights.cbegin(); }
		const_iterator end() { return scene._lights.cend(); }
	};

	LightIterableConst lights() const { return LightIterableConst(*this); }
public:
	float refractive_index = 1.000277f;
private:
	SceneNode _root_node;

	//std::vector<VertexSceneData> _vertices;
	//std::vector<TriangleSceneData> _triangle_indices;
	//std::vector<MeshSceneData> _meshes;
	//std::vector<Material> _meshes_materials;

	std::vector<Light> _lights;
};
}