#pragma once
#include <vector>
#include "shapes.h"
#include "ray.h"
#include "material.h"
#include "light.h"
#include "mesh.h"

#define MAX_TRACE_DEPTH 6

namespace raytracer {

class Scene
{
public:
	Scene();

	struct TriangleSceneData
	{
		glm::u32vec3 indices;
		u32 material_index;
	};

	void add_primitive(const Sphere& primitive, const Material& material) {
		_spheres.push_back(primitive);
		_sphere_materials.push_back(material);
	}

	void add_primitive(const Plane& primitive, const Material& material) {
		_planes.push_back(primitive);
		_planes_materials.push_back(material);
	}

	void add_primitive(const Mesh& primitive, const Material& material) {
		u32 starting_vertex_index = _vertices.size();
		u32 material_index = _meshes_materials.size();
		_meshes_materials.push_back(material);
		for (auto& triangleVertexIndices : primitive._triangleIndices) {
			TriangleSceneData triangleData;
			triangleData.indices = triangleVertexIndices + glm::u32vec3(starting_vertex_index);
			triangleData.material_index = material_index;
			_triangle_indices.push_back(triangleData);
		}
		for (auto& vertex : primitive._vertices) _vertices.push_back(vertex);
	}
	
	void add_light(Light& light)
	{
		_lights.push_back(light);
	}

	const std::vector<Sphere>& GetSpheres() const { return _spheres; };
	const std::vector<Material>& GetSphereMaterials() const { return _sphere_materials; };

	const std::vector<Plane>& GetPlanes() const { return _planes; };
	const std::vector<Material>& GetPlaneMaterials() const { return _planes_materials; };

	const std::vector<Light>& GetLights() const { return _lights; };
	const std::vector<glm::vec4> GetVertices() const { return _vertices; }
	const std::vector<Material> GetMeshMaterials() const { return _meshes_materials; }
	const std::vector<TriangleSceneData> GetTriangleIndices() const { return _triangle_indices; }
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
	std::vector<Sphere> _spheres;
	std::vector<Material> _sphere_materials;

	std::vector<Plane> _planes;
	std::vector<Material> _planes_materials;

	std::vector<glm::vec4> _vertices;
	std::vector<TriangleSceneData> _triangle_indices;
	std::vector<Material> _meshes_materials;

	std::vector<Light> _lights;
};
}