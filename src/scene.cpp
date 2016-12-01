#include "scene.h"
#include <algorithm>
#include <iostream>

using namespace raytracer;

raytracer::Scene::Scene() {
	_spheres.reserve(256);
	_sphere_materials.reserve(256);
}

void raytracer::Scene::add_primitive(const Mesh& primitive, const Material& material) {
	u32 starting_vertex_index = _vertices.size();
	int i = 0;
	for (auto& vertex : primitive._vertices) {
		_vertices.push_back(vertex);
		_normals.push_back(primitive._normals[i]);
		i++;
	}
	u32 material_index = _meshes_materials.size();
	_meshes_materials.push_back(material);
	for (auto& face : primitive._faces) {
		TriangleSceneData triangleData;
		triangleData.indices = face + glm::u32vec3(starting_vertex_index);
		triangleData.material_index = material_index;
		for (int i = 0; i < 3; ++i) {
			if (triangleData.indices[i] >= _vertices.size()) std::cout << "Error! invalid vertex in mesh!" << std::endl;
		}
		_triangle_indices.push_back(triangleData);
	}
	
}
