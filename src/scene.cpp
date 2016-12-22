#include "scene.h"
#include <algorithm>
#include <iostream>
#include <crtdbg.h>

using namespace raytracer;

raytracer::SceneNode& raytracer::Scene::add_node(const Mesh& primitive, const Transform& transform, SceneNode* parent /*= nullptr*/) {
	//return add_node(std::vector<Mesh>{ primitive }, transform, parent );
	if (!parent) parent = &get_root_node();

	_ASSERT(primitive.isValid());

	auto newChild = std::make_unique<SceneNode>();
	newChild->transform = transform;
	newChild->parent = parent;	
	SceneNode& newChildRef = *newChild.get();
	newChild->meshData.start_triangle_index = primitive.getFirstTriangleIndex();
	newChild->meshData.triangle_count = primitive.getTriangleCount();
	newChild->meshData.bvhIndex = primitive.getBvhRoot();
	parent->children.push_back(std::move(newChild));
	return newChildRef;
}

/*raytracer::SceneNode& raytracer::Scene::add_node(const std::vector<Mesh>& primitives, const Transform& transform, SceneNode* parent) {
	if (!parent) parent = &get_root_node();

	auto newChild = std::make_unique<SceneNode>();
	newChild->transform = transform;
	newChild->parent = parent;
	SceneNode& newChildRef = *newChild.get();
	u32 starting_triangle_index = _triangle_indices.size();
	u32 triangle_count = 0;
	for (auto& primitive : primitives) {
		if (primitive.getFaces().size() == 0) continue;
		triangle_count += primitive.getFaces().size();

		u32 starting_vertex_index = _vertices.size();
		int i = 0;
		for (auto& vertex : primitive.getVertices()) {
			VertexSceneData vertexData;
			vertexData.vertex = vertex;
			vertexData.normal = primitive.getNormals()[i];
			vertexData.texCoord = primitive.getTextureCoords()[i];
			_vertices.push_back(vertexData);
			i++;
		}

		u32 material_index = _meshes_materials.size();
		auto material = &primitive.getMaterial();
		_meshes_materials.push_back(*material);
		for (auto& face : primitive.getFaces()) {
			TriangleSceneData triangleData;
			triangleData.indices = face + glm::u32vec3(starting_vertex_index);
			triangleData.material_index = material_index;
			for (int i = 0; i < 3; ++i) {
				if (triangleData.indices[i] >= _vertices.size()) std::cout << "Error! invalid vertex in mesh!" << std::endl;
			}
			_triangle_indices.push_back(triangleData);
		}
	}
	newChild->meshData.start_triangle_index = starting_triangle_index;
	newChild->meshData.triangle_count = triangle_count;
	parent->children.push_back(std::move(newChild));
	return newChildRef;
}*/
