#pragma once
#include <vector>
#include "shapes.h"
#include "ray.h"
#include "material.h"
#include "light.h"
#include "mesh.h"
#include "bvh.h"

#define MAX_TRACE_DEPTH 6

namespace raytracer {

struct MeshSceneData
{
	u32 start_triangle_index;
	u32 triangle_count;
	i32 thinBvhIndex;
	bool is_empty() { return triangle_count == 0; }
	MeshSceneData(u32 startIndex = 0, u32 count = 0) : start_triangle_index(startIndex), triangle_count(count), thinBvhIndex(-1) {}
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

	Scene();

	struct TriangleSceneData
	{
		glm::u32vec3 indices;
		u32 material_index;
	};

	struct VertexSceneData
	{
		glm::vec4 vertex;
		glm::vec4 normal;
		glm::vec2 texCoord;
		byte __padding[8];
	}; 

	void add_primitive(const Sphere& primitive, const Material& material) {
		_spheres.push_back(primitive);
		_sphere_materials.push_back(material);
	}

	void add_primitive(const Plane& primitive, const Material& material) {
		_planes.push_back(primitive);
		_planes_materials.push_back(material);
	}

	SceneNode& add_node(const Mesh& primitive, const Transform& transform = Transform(), SceneNode* parent = nullptr);
	SceneNode& add_node(const std::vector<Mesh>& primitive, const Transform& transform = Transform(), SceneNode* parent = nullptr);

	void add_light(Light& light)
	{
		_lights.push_back(light);
	}

	SceneNode& get_root_node() { return _root_node; }
	const std::vector<Sphere>& GetSpheres() const { return _spheres; };
	const std::vector<Material>& GetSphereMaterials() const { return _sphere_materials; };

	const std::vector<Plane>& GetPlanes() const { return _planes; };
	const std::vector<Material>& GetPlaneMaterials() const { return _planes_materials; };

	const std::vector<Light>& GetLights() const { return _lights; };
	const std::vector<VertexSceneData> GetVertices() const { return _vertices; }
	const std::vector<TriangleSceneData> GetTriangleIndices() const { return _triangle_indices; }
	const std::vector<Material> GetMeshMaterials() const { return _meshes_materials; }

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

	std::vector<Sphere> _spheres;
	std::vector<Material> _sphere_materials;

	std::vector<Plane> _planes;
	std::vector<Material> _planes_materials;

	std::vector<VertexSceneData> _vertices;
	std::vector<TriangleSceneData> _triangle_indices;
	std::vector<MeshSceneData> _meshes;
	std::vector<Material> _meshes_materials;

	std::vector<Light> _lights;
};
}