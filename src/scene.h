#pragma once
#include <vector>
#include <memory>
#include "shapes.h"
#include "ray.h"
#include "material.h"
#include "light.h"
#include "mesh.h"

#define MAX_TRACE_DEPTH 6

namespace raytracer {

struct SceneNode
{
	SceneNode* parent = nullptr;
	std::vector<std::unique_ptr<SceneNode>> children;
	Transform transform;
	u32 mesh = -1;
};

struct MeshBvhPair
{
	MeshBvhPair(std::shared_ptr<Mesh>& mesh, u32 offset)
	{
		this->mesh = mesh;
		this->bvh_offset = offset;
	}
	std::shared_ptr<Mesh> mesh;
	u32 bvh_offset;// Offset of meshes bvh into the global bvh array
};

class Scene
{
public:
	friend class Bvh;

	Scene() { };

	SceneNode& add_node(const std::shared_ptr<Mesh> primitive, const Transform& transform = Transform(), SceneNode* parent = nullptr);
	//SceneNode& add_node(const std::vector<Mesh>& primitive, const Transform& transform = Transform(), SceneNode* parent = nullptr);

	void add_light(Light& light)
	{
		_lights.push_back(light);
	}

	SceneNode& get_root_node() { return _root_node; }

	const std::vector<Light>& get_lights() const { return _lights; };
	std::vector<MeshBvhPair>& get_meshes() { return _meshes; };
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

	std::vector<Light> _lights;
	std::vector<MeshBvhPair> _meshes;
};
}